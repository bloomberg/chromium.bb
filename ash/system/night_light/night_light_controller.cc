// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/night_light/night_light_controller.h"

#include <cmath>
#include <memory>

#include "ash/display/display_color_manager.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/icu/source/i18n/astro.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"

namespace ash {

namespace {

// Default start time at 6:00 PM as an offset from 00:00.
constexpr int kDefaultStartTimeOffsetMinutes = 18 * 60;

// Default end time at 6:00 AM as an offset from 00:00.
constexpr int kDefaultEndTimeOffsetMinutes = 6 * 60;

constexpr float kDefaultColorTemperature = 0.5f;

// The duration of the temperature change animation for
// AnimationDurationType::kShort.
constexpr base::TimeDelta kManualAnimationDuration =
    base::TimeDelta::FromSeconds(1);

// The duration of the temperature change animation for
// AnimationDurationType::kLong.
constexpr base::TimeDelta kAutomaticAnimationDuration =
    base::TimeDelta::FromSeconds(20);

// The color temperature animation frames per second.
constexpr int kNightLightAnimationFrameRate = 30;

class NightLightControllerDelegateImpl : public NightLightController::Delegate {
 public:
  NightLightControllerDelegateImpl() = default;
  ~NightLightControllerDelegateImpl() override = default;

  // ash::NightLightController::Delegate:
  base::Time GetNow() const override { return base::Time::Now(); }
  base::Time GetSunsetTime() const override { return GetSunRiseSet(false); }
  base::Time GetSunriseTime() const override { return GetSunRiseSet(true); }
  void SetGeoposition(mojom::SimpleGeopositionPtr position) override {
    geoposition_ = std::move(position);
  }
  bool HasGeoposition() const override { return !!geoposition_; }

 private:
  // Note that the below computation is intentionally performed every time
  // GetSunsetTime() or GetSunriseTime() is called rather than once whenever we
  // receive a geoposition (which happens at least once a day). This increases
  // the chances of getting accurate values, especially around DST changes.
  base::Time GetSunRiseSet(bool sunrise) const {
    if (!HasGeoposition()) {
      LOG(ERROR) << "Invalid geoposition. Using default time for "
                 << (sunrise ? "sunrise." : "sunset.");
      return sunrise ? TimeOfDay(kDefaultEndTimeOffsetMinutes).ToTimeToday()
                     : TimeOfDay(kDefaultStartTimeOffsetMinutes).ToTimeToday();
    }

    icu::CalendarAstronomer astro(geoposition_->longitude,
                                  geoposition_->latitude);
    // For sunset and sunrise times calculations to be correct, the time of the
    // icu::CalendarAstronomer object should be set to a time near local noon.
    // This avoids having the computation flopping over into an adjacent day.
    // See the documentation of icu::CalendarAstronomer::getSunRiseSet().
    // Note that the icu calendar works with milliseconds since epoch, and
    // base::Time::FromDoubleT() / ToDoubleT() work with seconds since epoch.
    const double midday_today_sec =
        TimeOfDay(12 * 60).ToTimeToday().ToDoubleT();
    astro.setTime(midday_today_sec * 1000.0);
    const double sun_rise_set_ms = astro.getSunRiseSet(sunrise);
    return base::Time::FromDoubleT(sun_rise_set_ms / 1000.0);
  }

  mojom::SimpleGeopositionPtr geoposition_;

  DISALLOW_COPY_AND_ASSIGN(NightLightControllerDelegateImpl);
};

// Returns the color temperature range bucket in which |temperature| resides.
// The range buckets are:
// 0 => Range [0 : 20) (least warm).
// 1 => Range [20 : 40).
// 2 => Range [40 : 60).
// 3 => Range [60 : 80).
// 4 => Range [80 : 100) (most warm).
int GetTemperatureRange(float temperature) {
  return std::floor(5 * temperature);
}

// Returns the color matrix that corresponds to the given |temperature|.
// If |in_linear_gamma_space| is true, the generated matrix is the one that
// should be applied after gamma correction, and it corresponds to the
// non-linear temperature value for the given |temperature|.
SkMatrix44 MatrixFromTemperature(float temperature,
                                 bool in_linear_gamma_space) {
  if (in_linear_gamma_space)
    temperature = NightLightController::GetNonLinearTemperature(temperature);

  SkMatrix44 matrix(SkMatrix44::kIdentity_Constructor);
  if (temperature != 0.0f) {
    const float blue_scale =
        NightLightController::BlueColorScaleFromTemperature(temperature);
    const float green_scale =
        NightLightController::GreenColorScaleFromTemperature(
            temperature, in_linear_gamma_space);

    matrix.set(1, 1, green_scale);
    matrix.set(2, 2, blue_scale);
  }
  return matrix;
}

// Based on the result of setting the hardware CRTC matrix |crtc_matrix_result|,
// either apply the |night_light_matrix| on the compositor, or reset it to
// the identity matrix to avoid having double the Night Light effect.
void UpdateCompositorMatrix(aura::WindowTreeHost* host,
                            const SkMatrix44& night_light_matrix,
                            bool crtc_matrix_result) {
  if (host->compositor()) {
    host->compositor()->SetDisplayColorMatrix(
        crtc_matrix_result ? SkMatrix44::I() : night_light_matrix);
  }
}

// Attempts setting one of the given color matrices on the display hardware of
// |display_id| depending on the hardware capability. The matrix
// |linear_gamma_space_matrix| will be applied if the hardware applies the
// CTM in the linear gamma space (i.e. after gamma decoding), whereas the
// matrix |gamma_compressed_matrix| will be applied instead if the hardware
// applies the CTM in the gamma compressed space (i.e. after degamma
// encoding).
// Returns true if the hardware supports this operation and one of the
// matrices was successfully sent to the GPU.
bool AttemptSettingHardwareCtm(int64_t display_id,
                               const SkMatrix44& linear_gamma_space_matrix,
                               const SkMatrix44& gamma_compressed_matrix) {
  for (const auto* snapshot :
       Shell::Get()->display_configurator()->cached_displays()) {
    if (snapshot->display_id() == display_id &&
        snapshot->has_color_correction_matrix()) {
      return Shell::Get()->display_color_manager()->SetDisplayColorMatrix(
          snapshot, snapshot->color_correction_in_linear_space()
                        ? linear_gamma_space_matrix
                        : gamma_compressed_matrix);
    }
  }

  return false;
}

// Applies the given |temperature| to the display associated with the given
// |host|. This is useful for when we have a host and not a display ID.
void ApplyTemperatureToHost(aura::WindowTreeHost* host, float temperature) {
  DCHECK(host);
  const int64_t display_id = host->GetDisplayId();
  DCHECK_NE(display_id, display::kInvalidDisplayId);
  if (display_id == display::kUnifiedDisplayId) {
    // In Unified Desktop mode, applying the color matrix to either the CRTC or
    // the compositor of the mirroring (actual) displays is sufficient. If we
    // apply it to the compositor of the Unified host (since there's no actual
    // display CRTC for |display::kUnifiedDisplayId|), then we'll end up with
    // double the temperature.
    return;
  }

  const SkMatrix44 linear_gamma_space_matrix =
      MatrixFromTemperature(temperature, true);
  const SkMatrix44 gamma_compressed_matrix =
      MatrixFromTemperature(temperature, false);
  const bool crtc_result = AttemptSettingHardwareCtm(
      display_id, linear_gamma_space_matrix, gamma_compressed_matrix);
  UpdateCompositorMatrix(host, gamma_compressed_matrix, crtc_result);
}

// Applies the given |temperature| value by converting it to the corresponding
// color matrix that will be set on the output displays.
void ApplyTemperatureToAllDisplays(float temperature) {
  const SkMatrix44 linear_gamma_space_matrix =
      MatrixFromTemperature(temperature, true);
  const SkMatrix44 gamma_compressed_matrix =
      MatrixFromTemperature(temperature, false);

  Shell* shell = Shell::Get();
  WindowTreeHostManager* wth_manager = shell->window_tree_host_manager();
  for (int64_t display_id :
       shell->display_manager()->GetCurrentDisplayIdList()) {
    DCHECK_NE(display_id, display::kUnifiedDisplayId);

    const bool crtc_result = AttemptSettingHardwareCtm(
        display_id, linear_gamma_space_matrix, gamma_compressed_matrix);

    aura::Window* root_window =
        wth_manager->GetRootWindowForDisplayId(display_id);
    if (!root_window) {
      // Some displays' hosts may have not being initialized yet. In this case
      // NightLightController::OnHostInitialized() will take care of those
      // hosts.
      continue;
    }

    auto* host = root_window->GetHost();
    DCHECK(host);
    UpdateCompositorMatrix(host, gamma_compressed_matrix, crtc_result);
  }
}

}  // namespace

// Defines a linear animation type to animate the color temperature between two
// values in a given time duration. The color temperature is animated when
// NightLight changes status from ON to OFF or vice versa, whether this change
// is automatic (via the automatic schedule) or manual (user initiated).
class ColorTemperatureAnimation : public gfx::LinearAnimation,
                                  public gfx::AnimationDelegate {
 public:
  ColorTemperatureAnimation()
      : gfx::LinearAnimation(kManualAnimationDuration,
                             kNightLightAnimationFrameRate,
                             this) {}
  ~ColorTemperatureAnimation() override = default;

  // Starts a new temperature animation from the |current_temperature_| to the
  // given |new_target_temperature| in the given |duration|.
  void AnimateToNewValue(float new_target_temperature,
                         base::TimeDelta duration) {
    start_temperature_ = current_temperature_;
    target_temperature_ =
        base::ClampToRange(new_target_temperature, 0.0f, 1.0f);

    if (ui::ScopedAnimationDurationScaleMode::duration_scale_mode() ==
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {
      // Animations are disabled. Apply the target temperature directly to the
      // compositors.
      current_temperature_ = target_temperature_;
      ApplyTemperatureToAllDisplays(target_temperature_);
      Stop();
      return;
    }

    SetDuration(duration);
    Start();
  }

 private:
  // gfx::Animation:
  void AnimateToState(double state) override {
    state = base::ClampToRange(state, 0.0, 1.0);
    current_temperature_ =
        start_temperature_ + (target_temperature_ - start_temperature_) * state;
  }

  // gfx::AnimationDelegate:
  void AnimationProgressed(const Animation* animation) override {
    DCHECK_EQ(animation, this);

    if (std::abs(current_temperature_ - target_temperature_) <=
        std::numeric_limits<float>::epsilon()) {
      current_temperature_ = target_temperature_;
      Stop();
    }

    ApplyTemperatureToAllDisplays(current_temperature_);
  }

  float start_temperature_ = 0.0f;
  float current_temperature_ = 0.0f;
  float target_temperature_ = 0.0f;

  DISALLOW_COPY_AND_ASSIGN(ColorTemperatureAnimation);
};

NightLightController::NightLightController()
    : delegate_(std::make_unique<NightLightControllerDelegateImpl>()),
      temperature_animation_(std::make_unique<ColorTemperatureAnimation>()),
      binding_(this) {
  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->window_tree_host_manager()->AddObserver(this);
  aura::Env::GetInstance()->AddObserver(this);
  chromeos::PowerManagerClient::Get()->AddObserver(this);
}

NightLightController::~NightLightController() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  aura::Env::GetInstance()->RemoveObserver(this);
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
void NightLightController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kNightLightEnabled, false,
                                PrefRegistry::PUBLIC);
  registry->RegisterDoublePref(prefs::kNightLightTemperature,
                               kDefaultColorTemperature, PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kNightLightScheduleType,
                                static_cast<int>(ScheduleType::kNone),
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kNightLightCustomStartTime,
                                kDefaultStartTimeOffsetMinutes,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kNightLightCustomEndTime,
                                kDefaultEndTimeOffsetMinutes,
                                PrefRegistry::PUBLIC);

  // Non-public prefs, only meant to be used by ash.
  registry->RegisterDoublePref(prefs::kNightLightCachedLatitude, 0.0);
  registry->RegisterDoublePref(prefs::kNightLightCachedLongitude, 0.0);
}

// static
float NightLightController::BlueColorScaleFromTemperature(float temperature) {
  return 1.0f - temperature;
}

// static
float NightLightController::GreenColorScaleFromTemperature(
    float temperature,
    bool in_linear_space) {
  // If we only tone down the blue scale, the screen will look very green so
  // we also need to tone down the green, but with a less value compared to
  // the blue scale to avoid making things look very red.
  return 1.0f - (in_linear_space ? 0.7f : 0.5f) * temperature;
}

// static
float NightLightController::GetNonLinearTemperature(float temperature) {
  constexpr float kGammaFactor = 1.0f / 2.2f;
  return std::pow(temperature, kGammaFactor);
}

void NightLightController::BindRequest(
    mojom::NightLightControllerRequest request) {
  binding_.Bind(std::move(request));
}

void NightLightController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NightLightController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool NightLightController::GetEnabled() const {
  return active_user_pref_service_ &&
         active_user_pref_service_->GetBoolean(prefs::kNightLightEnabled);
}

float NightLightController::GetColorTemperature() const {
  if (active_user_pref_service_)
    return active_user_pref_service_->GetDouble(prefs::kNightLightTemperature);

  return kDefaultColorTemperature;
}

NightLightController::ScheduleType NightLightController::GetScheduleType()
    const {
  if (active_user_pref_service_) {
    return static_cast<ScheduleType>(
        active_user_pref_service_->GetInteger(prefs::kNightLightScheduleType));
  }

  return ScheduleType::kNone;
}

TimeOfDay NightLightController::GetCustomStartTime() const {
  if (active_user_pref_service_) {
    return TimeOfDay(active_user_pref_service_->GetInteger(
        prefs::kNightLightCustomStartTime));
  }

  return TimeOfDay(kDefaultStartTimeOffsetMinutes);
}

TimeOfDay NightLightController::GetCustomEndTime() const {
  if (active_user_pref_service_) {
    return TimeOfDay(
        active_user_pref_service_->GetInteger(prefs::kNightLightCustomEndTime));
  }

  return TimeOfDay(kDefaultEndTimeOffsetMinutes);
}

void NightLightController::SetEnabled(bool enabled,
                                      AnimationDuration animation_type) {
  if (active_user_pref_service_) {
    animation_duration_ = animation_type;
    active_user_pref_service_->SetBoolean(prefs::kNightLightEnabled, enabled);
  }
}

void NightLightController::SetColorTemperature(float temperature) {
  DCHECK_GE(temperature, 0.0f);
  DCHECK_LE(temperature, 1.0f);
  if (active_user_pref_service_) {
    active_user_pref_service_->SetDouble(prefs::kNightLightTemperature,
                                         temperature);
  }
}

void NightLightController::SetScheduleType(ScheduleType type) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetInteger(prefs::kNightLightScheduleType,
                                          static_cast<int>(type));
  }
}

void NightLightController::SetCustomStartTime(TimeOfDay start_time) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetInteger(
        prefs::kNightLightCustomStartTime,
        start_time.offset_minutes_from_zero_hour());
  }
}

void NightLightController::SetCustomEndTime(TimeOfDay end_time) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetInteger(
        prefs::kNightLightCustomEndTime,
        end_time.offset_minutes_from_zero_hour());
  }
}

void NightLightController::Toggle() {
  SetEnabled(!GetEnabled(), AnimationDuration::kShort);
}

void NightLightController::OnDisplayConfigurationChanged() {
  // When display configurations changes, we should re-apply the current
  // temperature immediately without animation.
  ApplyTemperatureToAllDisplays(GetEnabled() ? GetColorTemperature() : 0.0f);
}

void NightLightController::OnHostInitialized(aura::WindowTreeHost* host) {
  // This newly initialized |host| could be of a newly added display, or of a
  // newly created mirroring display (either for mirroring or unified). we need
  // to apply the current temperature immediately without animation.
  ApplyTemperatureToHost(host, GetEnabled() ? GetColorTemperature() : 0.0f);
}

void NightLightController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  // Initial login and user switching in multi profiles.
  active_user_pref_service_ = pref_service;
  InitFromUserPrefs();
}

void NightLightController::SetCurrentGeoposition(
    mojom::SimpleGeopositionPtr position) {
  VLOG(1) << "Received new geoposition.";

  is_current_geoposition_from_cache_ = false;
  StoreCachedGeoposition(position);
  delegate_->SetGeoposition(std::move(position));

  // If the schedule type is sunset to sunrise, then a potential change in the
  // geoposition might mean a change in the start and end times. In this case,
  // we must trigger a refresh.
  if (GetScheduleType() == ScheduleType::kSunsetToSunrise)
    Refresh(true /* did_schedule_change */);
}

void NightLightController::SetClient(mojom::NightLightClientPtr client) {
  client_ = std::move(client);

  if (client_) {
    client_.set_connection_error_handler(base::Bind(
        &NightLightController::SetClient, base::Unretained(this), nullptr));
    NotifyClientWithScheduleChange();
  }
}

void NightLightController::SuspendDone(const base::TimeDelta& sleep_duration) {
  // Time changes while the device is suspended. We need to refresh the schedule
  // upon device resume to know what the status should be now.
  Refresh(true /* did_schedule_change */);
}

void NightLightController::SetDelegateForTesting(
    std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

void NightLightController::LoadCachedGeopositionIfNeeded() {
  DCHECK(active_user_pref_service_);

  // Even if there is a geoposition, but it's coming from a previously cached
  // value, switching users should load the currently saved values for the
  // new user. This is to keep users' prefs completely separate. We only ignore
  // the cached values once we have a valid non-cached geoposition from any
  // user in the same session.
  if (delegate_->HasGeoposition() && !is_current_geoposition_from_cache_)
    return;

  if (!active_user_pref_service_->HasPrefPath(
          prefs::kNightLightCachedLatitude) ||
      !active_user_pref_service_->HasPrefPath(
          prefs::kNightLightCachedLongitude)) {
    VLOG(1) << "No valid current geoposition and no valid cached geoposition"
               " are available. Will use default times for sunset / sunrise.";
    return;
  }

  VLOG(1) << "Temporarily using a previously cached geoposition.";
  delegate_->SetGeoposition(mojom::SimpleGeoposition::New(
      active_user_pref_service_->GetDouble(prefs::kNightLightCachedLatitude),
      active_user_pref_service_->GetDouble(prefs::kNightLightCachedLongitude)));
  is_current_geoposition_from_cache_ = true;
}

void NightLightController::StoreCachedGeoposition(
    const mojom::SimpleGeopositionPtr& position) {
  DCHECK(position);

  const SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  for (const auto& user_session : session_controller->GetUserSessions()) {
    PrefService* pref_service = session_controller->GetUserPrefServiceForUser(
        user_session->user_info.account_id);
    if (!pref_service)
      continue;

    pref_service->SetDouble(prefs::kNightLightCachedLatitude,
                            position->latitude);
    pref_service->SetDouble(prefs::kNightLightCachedLongitude,
                            position->longitude);
  }
}

void NightLightController::RefreshLayersTemperature() {
  const float new_temperature = GetEnabled() ? GetColorTemperature() : 0.0f;
  temperature_animation_->AnimateToNewValue(
      new_temperature, animation_duration_ == AnimationDuration::kShort
                           ? kManualAnimationDuration
                           : kAutomaticAnimationDuration);

  UMA_HISTOGRAM_EXACT_LINEAR(
      "Ash.NightLight.Temperature", GetTemperatureRange(new_temperature),
      5 /* number of buckets defined in GetTemperatureRange() */);

  // Reset the animation type back to manual to consume any automatically set
  // animations.
  last_animation_duration_ = animation_duration_;
  animation_duration_ = AnimationDuration::kShort;
  Shell::Get()->UpdateCursorCompositingEnabled();
}

void NightLightController::StartWatchingPrefsChanges() {
  DCHECK(active_user_pref_service_);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(active_user_pref_service_);
  pref_change_registrar_->Add(
      prefs::kNightLightEnabled,
      base::BindRepeating(&NightLightController::OnEnabledPrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kNightLightTemperature,
      base::BindRepeating(&NightLightController::OnColorTemperaturePrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kNightLightScheduleType,
      base::BindRepeating(&NightLightController::OnScheduleTypePrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kNightLightCustomStartTime,
      base::BindRepeating(&NightLightController::OnCustomSchedulePrefsChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kNightLightCustomEndTime,
      base::BindRepeating(&NightLightController::OnCustomSchedulePrefsChanged,
                          base::Unretained(this)));

  // Note: No need to observe changes in the cached latitude/longitude since
  // they're only accessed here in ash. We only load them when the active user
  // changes, and store them whenever we receive an updated geoposition.
}

void NightLightController::InitFromUserPrefs() {
  StartWatchingPrefsChanges();
  LoadCachedGeopositionIfNeeded();
  Refresh(true /* did_schedule_change */);
  NotifyStatusChanged();
  NotifyClientWithScheduleChange();
}

void NightLightController::NotifyStatusChanged() {
  for (auto& observer : observers_)
    observer.OnNightLightEnabledChanged(GetEnabled());
}

void NightLightController::NotifyClientWithScheduleChange() {
  if (client_)
    client_->OnScheduleTypeChanged(GetScheduleType());
}

void NightLightController::OnEnabledPrefChanged() {
  DCHECK(active_user_pref_service_);
  Refresh(false /* did_schedule_change */);
  NotifyStatusChanged();
}

void NightLightController::OnColorTemperaturePrefChanged() {
  DCHECK(active_user_pref_service_);
  RefreshLayersTemperature();
}

void NightLightController::OnScheduleTypePrefChanged() {
  DCHECK(active_user_pref_service_);
  NotifyClientWithScheduleChange();
  Refresh(true /* did_schedule_change */);

  UMA_HISTOGRAM_ENUMERATION("Ash.NightLight.ScheduleType", GetScheduleType());
}

void NightLightController::OnCustomSchedulePrefsChanged() {
  DCHECK(active_user_pref_service_);
  Refresh(true /* did_schedule_change */);
}

void NightLightController::Refresh(bool did_schedule_change) {
  RefreshLayersTemperature();

  const ScheduleType type = GetScheduleType();
  switch (type) {
    case ScheduleType::kNone:
      timer_.Stop();
      return;

    case ScheduleType::kSunsetToSunrise:
      RefreshScheduleTimer(delegate_->GetSunsetTime(),
                           delegate_->GetSunriseTime(), did_schedule_change);
      return;

    case ScheduleType::kCustom:
      RefreshScheduleTimer(GetCustomStartTime().ToTimeToday(),
                           GetCustomEndTime().ToTimeToday(),
                           did_schedule_change);
      return;
  }
}

void NightLightController::RefreshScheduleTimer(base::Time start_time,
                                                base::Time end_time,
                                                bool did_schedule_change) {
  if (GetScheduleType() == ScheduleType::kNone) {
    NOTREACHED();
    timer_.Stop();
    return;
  }

  // NOTE: Users can set any weird combinations.
  const base::Time now = delegate_->GetNow();
  if (end_time <= start_time) {
    // Example:
    // Start: 9:00 PM, End: 6:00 AM.
    //
    //       6:00                21:00
    // <----- + ------------------ + ----->
    //        |                    |
    //       end                 start
    //
    // Note that the above times are times of day (today). It is important to
    // know where "now" is with respect to these times to decide how to adjust
    // them.
    if (end_time >= now) {
      // If the end time (today) is greater than the time now, this means "now"
      // is within the NightLight schedule, and the start time is actually
      // yesterday. The above timeline is interpreted as:
      //
      //   21:00 (-1day)              6:00
      // <----- + ----------- + ------ + ----->
      //        |             |        |
      //      start          now      end
      //
      start_time -= base::TimeDelta::FromDays(1);
    } else {
      // Two possibilities here:
      // - Either "now" is greater than the end time, but less than start time.
      //   This means NightLight is outside the schedule, waiting for the next
      //   start time. The end time is actually a day later.
      // - Or "now" is greater than both the start and end times. This means
      //   NightLight is within the schedule, waiting to turn off at the next
      //   end time, which is also a day later.
      end_time += base::TimeDelta::FromDays(1);
    }
  }

  DCHECK_GE(end_time, start_time);

  // The target status that we need to set NightLight to now if a change of
  // status is needed immediately.
  bool enable_now = false;

  // Where are we now with respect to the start and end times?
  if (now < start_time) {
    // Example:
    // Start: 6:00 PM today, End: 6:00 AM tomorrow, Now: 4:00 PM.
    //
    // <----- + ----------- + ----------- + ----->
    //        |             |             |
    //       now          start          end
    //
    // In this case, we need to disable NightLight immediately if it's enabled.
    enable_now = false;
  } else if (now >= start_time && now < end_time) {
    // Example:
    // Start: 6:00 PM today, End: 6:00 AM tomorrow, Now: 11:00 PM.
    //
    // <----- + ----------- + ----------- + ----->
    //        |             |             |
    //      start          now           end
    //
    // Start NightLight right away. Our future start time is a day later than
    // its current value.
    enable_now = true;
    start_time += base::TimeDelta::FromDays(1);
  } else {  // now >= end_time.
    // Example:
    // Start: 6:00 PM today, End: 10:00 PM today, Now: 11:00 PM.
    //
    // <----- + ----------- + ----------- + ----->
    //        |             |             |
    //      start          end           now
    //
    // In this case, our future start and end times are a day later from their
    // current values. NightLight needs to be ended immediately if it's already
    // enabled.
    enable_now = false;
    start_time += base::TimeDelta::FromDays(1);
    end_time += base::TimeDelta::FromDays(1);
  }

  // After the above processing, the start and end time are all in the future.
  DCHECK_GE(start_time, now);
  DCHECK_GE(end_time, now);

  if (did_schedule_change && enable_now != GetEnabled()) {
    // If the change in the schedule introduces a change in the status, then
    // calling SetEnabled() is all we need, since it will trigger a change in
    // the user prefs to which we will respond by calling Refresh(). This will
    // end up in this function again, adjusting all the needed schedules.
    SetEnabled(enable_now, AnimationDuration::kShort);
    return;
  }

  // We reach here in one of the following conditions:
  // 1) If schedule changes don't result in changes in the status, we need to
  // explicitly update the timer to re-schedule the next toggle to account for
  // any changes.
  // 2) The user has just manually toggled the status of NightLight either from
  // the System Menu or System Settings. In this case, we respect the user
  // wish and maintain the current status that they desire, but we schedule the
  // status to be toggled according to the time that corresponds with the
  // opposite status of the current one.
  ScheduleNextToggle(GetEnabled() ? end_time - now : start_time - now);
}

void NightLightController::ScheduleNextToggle(base::TimeDelta delay) {
  const bool new_status = !GetEnabled();
  VLOG(1) << "Setting Night Light to toggle to "
          << (new_status ? "enabled" : "disabled") << " at "
          << base::TimeFormatTimeOfDay(delegate_->GetNow() + delay);
  timer_.Start(
      FROM_HERE, delay,
      base::Bind(&NightLightController::SetEnabled, base::Unretained(this),
                 new_status, AnimationDuration::kLong));
}

}  // namespace ash
