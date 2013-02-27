// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/risk/fingerprint.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/cpu.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/string_split.h"
#include "base/sys_info.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/risk/proto/fingerprint.pb.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/font_list_async.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/content_client.h"
#include "content/public/common/gpu_info.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScreenInfo.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "webkit/plugins/webplugininfo.h"

using WebKit::WebScreenInfo;

namespace autofill {
namespace risk {

namespace {

const int32 kFingerprinterVersion = 1;

// Returns the delta between the time at which Chrome was installed and the Unix
// epoch.
base::TimeDelta GetInstallTimestamp() {
  PrefService* prefs = g_browser_process->local_state();
  base::Time install_time =
      base::Time::FromTimeT(prefs->GetInt64(prefs::kInstallDate));
  // The install date should always be available and initialized.
  DCHECK(!install_time.is_null());
  return install_time - base::Time::UnixEpoch();
}

// Returns the delta between the local timezone and UTC.
base::TimeDelta GetTimezoneOffset() {
  base::Time utc = base::Time::Now();

  base::Time::Exploded local;
  utc.LocalExplode(&local);

  return base::Time::FromUTCExploded(local) - utc;
}

// Returns the concatenation of the operating system name and version, e.g.
// "Mac OS X 10.6.8".
std::string GetOperatingSystemVersion() {
  return base::SysInfo::OperatingSystemName() + " " +
      base::SysInfo::OperatingSystemVersion();
}

// Adds the list of |fonts| to the |machine|.
void AddFontsToFingerprint(const base::ListValue& fonts,
                           Fingerprint_MachineCharacteristics* machine) {
  for (base::ListValue::const_iterator it = fonts.begin();
       it != fonts.end(); ++it) {
    // Each item in the list is a two-element list such that the first element
    // is the font family and the second is the font name.
    const base::ListValue* font_description;
    bool success = (*it)->GetAsList(&font_description);
    DCHECK(success);

    std::string font_name;
    success = font_description->GetString(1, &font_name);
    DCHECK(success);

    machine->add_font(font_name);
  }
}

// Adds the list of |plugins| to the |machine|.
void AddPluginsToFingerprint(const std::vector<webkit::WebPluginInfo>& plugins,
                             Fingerprint_MachineCharacteristics* machine) {
  for (std::vector<webkit::WebPluginInfo>::const_iterator it = plugins.begin();
       it != plugins.end(); ++it) {
    Fingerprint_MachineCharacteristics_Plugin* plugin =
        machine->add_plugin();
    plugin->set_name(UTF16ToUTF8(it->name));
    plugin->set_description(UTF16ToUTF8(it->desc));
    for (std::vector<webkit::WebPluginMimeType>::const_iterator mime_type =
             it->mime_types.begin();
         mime_type != it->mime_types.end(); ++mime_type) {
      plugin->add_mime_type(mime_type->mime_type);
    }
    plugin->set_version(UTF16ToUTF8(it->version));
  }
}

// Adds the list of HTTP accept languages in |prefs| to the |machine|.
void AddAcceptLanguagesToFingerprint(
    const std::string& accept_languages_str,
    Fingerprint_MachineCharacteristics* machine) {
  std::vector<std::string> accept_languages;
  base::SplitString(accept_languages_str, ',', &accept_languages);
  for (std::vector<std::string>::const_iterator it = accept_languages.begin();
       it != accept_languages.end(); ++it) {
    machine->add_requested_language(*it);
  }
}

// Writes
//   (a) the number of screens,
//   (b) the primary display's screen size,
//   (c) the screen's color depth, and
//   (d) the size of the screen unavailable to web page content,
//       i.e. the Taskbar size on Windows
// into the |machine|.
void AddScreenInfoToFingerprint(const WebScreenInfo& screen_info,
                                Fingerprint_MachineCharacteristics* machine) {
  // TODO(scottmg): NativeScreen maybe wrong. http://crbug.com/133312
  machine->set_screen_count(
      gfx::Screen::GetNativeScreen()->GetNumDisplays());

  gfx::Size screen_size =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().GetSizeInPixel();
  machine->mutable_screen_size()->set_width(screen_size.width());
  machine->mutable_screen_size()->set_height(screen_size.height());

  machine->set_screen_color_depth(screen_info.depth);

  gfx::Rect screen_rect(screen_info.rect);
  gfx::Rect available_rect(screen_info.availableRect);
  gfx::Rect unavailable_rect = gfx::SubtractRects(screen_rect, available_rect);
  machine->mutable_unavailable_screen_size()->set_width(
      unavailable_rect.width());
  machine->mutable_unavailable_screen_size()->set_height(
      unavailable_rect.height());
}

// Writes info about the machine's CPU into the |machine|.
void AddCpuInfoToFingerprint(Fingerprint_MachineCharacteristics* machine) {
  base::CPU cpu;
  machine->mutable_cpu()->set_vendor_name(cpu.vendor_name());
  machine->mutable_cpu()->set_brand(cpu.cpu_brand());
}

// Writes info about the machine's GPU into the |machine|.
void AddGpuInfoToFingerprint(Fingerprint_MachineCharacteristics* machine) {
  const content::GPUInfo& gpu_info =
      content::GpuDataManager::GetInstance()->GetGPUInfo();
  DCHECK(gpu_info.finalized);

  Fingerprint_MachineCharacteristics_Graphics* graphics =
      machine->mutable_graphics_card();
  graphics->set_vendor_id(gpu_info.gpu.vendor_id);
  graphics->set_device_id(gpu_info.gpu.device_id);
  graphics->set_driver_version(gpu_info.driver_version);
  graphics->set_driver_date(gpu_info.driver_date);

  Fingerprint_MachineCharacteristics_Graphics_PerformanceStatistics*
      gpu_performance = graphics->mutable_performance_statistics();
  gpu_performance->set_graphics_score(gpu_info.performance_stats.graphics);
  gpu_performance->set_gaming_score(gpu_info.performance_stats.gaming);
  gpu_performance->set_overall_score(gpu_info.performance_stats.overall);
}

// Waits for all asynchronous data required for the fingerprint to be loaded;
// then fills out the fingerprint.
class FingerprintDataLoader : public content::GpuDataManagerObserver {
 public:
  FingerprintDataLoader(
      int64 gaia_id,
      const gfx::Rect& window_bounds,
      const gfx::Rect& content_bounds,
      const WebScreenInfo& screen_info,
      const PrefService& prefs,
      const base::Callback<void(scoped_ptr<Fingerprint>)>& callback);

 private:
  virtual ~FingerprintDataLoader();

  // content::GpuDataManagerObserver:
  virtual void OnGpuInfoUpdate() OVERRIDE;

  // Callbacks for asynchronously loaded data.
  void OnGotFonts(scoped_ptr<base::ListValue> fonts);
  void OnGotPlugins(const std::vector<webkit::WebPluginInfo>& plugins);

  // If all of the asynchronous data has been loaded, calls |callback_| with
  // the fingerprint data.
  void MaybeFillFingerprint();

  // Calls |callback_| with the fingerprint data.
  void FillFingerprint();

  // The GPU data provider.
  content::GpuDataManager* const gpu_data_manager_;

  // Data that will be passed on to the next loading phase.
  const int64 gaia_id_;
  const gfx::Rect window_bounds_;
  const gfx::Rect content_bounds_;
  const WebScreenInfo screen_info_;
  const std::string charset_;
  const std::string accept_languages_;

  // Data that will be loaded asynchronously.
  scoped_ptr<base::ListValue> fonts_;
  std::vector<webkit::WebPluginInfo> plugins_;
  bool has_loaded_plugins_;

  // The callback that will be called once all the data is available.
  base::Callback<void(scoped_ptr<Fingerprint>)> callback_;

  DISALLOW_COPY_AND_ASSIGN(FingerprintDataLoader);
};

FingerprintDataLoader::FingerprintDataLoader(
    int64 gaia_id,
    const gfx::Rect& window_bounds,
    const gfx::Rect& content_bounds,
    const WebScreenInfo& screen_info,
    const PrefService& prefs,
    const base::Callback<void(scoped_ptr<Fingerprint>)>& callback)
    : gpu_data_manager_(content::GpuDataManager::GetInstance()),
      gaia_id_(gaia_id),
      window_bounds_(window_bounds),
      content_bounds_(content_bounds),
      screen_info_(screen_info),
      charset_(prefs.GetString(prefs::kDefaultCharset)),
      accept_languages_(prefs.GetString(prefs::kAcceptLanguages)),
      has_loaded_plugins_(false),
      callback_(callback) {
  // TODO(isherman): Investigating http://crbug.com/174296
  LOG(WARNING) << "Loading fingerprint data.";

  // Load GPU data if needed.
  if (!gpu_data_manager_->IsCompleteGpuInfoAvailable()) {
    // TODO(isherman): Investigating http://crbug.com/174296
    LOG(WARNING) << "Loading GPU data.";

    gpu_data_manager_->AddObserver(this);
    gpu_data_manager_->RequestCompleteGpuInfoIfNeeded();
  } else {
    // TODO(isherman): Investigating http://crbug.com/174296
    LOG(WARNING) << "GPU data already loaded.";
  }

  // Load plugin data.
  content::PluginService::GetInstance()->GetPlugins(
      base::Bind(&FingerprintDataLoader::OnGotPlugins, base::Unretained(this)));

  // Load font data.
  content::GetFontListAsync(
      base::Bind(&FingerprintDataLoader::OnGotFonts, base::Unretained(this)));
}

FingerprintDataLoader::~FingerprintDataLoader() {
}

void FingerprintDataLoader::OnGpuInfoUpdate() {
  if (!gpu_data_manager_->IsCompleteGpuInfoAvailable()) {
    // TODO(isherman): Investigating http://crbug.com/174296
    LOG(WARNING) << "OnGpuInfoUpdate() called without complete GPU info.";
    return;
  }

  // TODO(isherman): Investigating http://crbug.com/174296
  LOG(WARNING) << "Loaded GPU data.";

  gpu_data_manager_->RemoveObserver(this);
  MaybeFillFingerprint();
}

void FingerprintDataLoader::OnGotFonts(scoped_ptr<base::ListValue> fonts) {
  // TODO(isherman): Investigating http://crbug.com/174296
  LOG(WARNING) << "Loaded fonts.";

  DCHECK(!fonts_);
  fonts_.reset(fonts.release());
  MaybeFillFingerprint();
}

void FingerprintDataLoader::OnGotPlugins(
    const std::vector<webkit::WebPluginInfo>& plugins) {
  // TODO(isherman): Investigating http://crbug.com/174296
  LOG(WARNING) << "Loaded plugins.";

  DCHECK(!has_loaded_plugins_);
  has_loaded_plugins_ = true;
  plugins_ = plugins;
  MaybeFillFingerprint();
}

void FingerprintDataLoader::MaybeFillFingerprint() {
  // TODO(isherman): Investigating http://crbug.com/174296
  LOG(WARNING) << "GPU data: "
               << (gpu_data_manager_->IsCompleteGpuInfoAvailable() ?
                       "loaded" :
                       "waiting");
  LOG(WARNING) << "Fonts: "
               << (fonts_ ? "loaded" : "waiting");
  LOG(WARNING) << "Plugins: "
               << (has_loaded_plugins_ ? "loaded" : "waiting");

  // If all of the data has been loaded, fill the fingerprint and clean up.
  if (gpu_data_manager_->IsCompleteGpuInfoAvailable() &&
      fonts_ &&
      has_loaded_plugins_) {
    FillFingerprint();
    delete this;
  }
}

void FingerprintDataLoader::FillFingerprint() {
  scoped_ptr<Fingerprint> fingerprint(new Fingerprint);
  Fingerprint_MachineCharacteristics* machine =
      fingerprint->mutable_machine_characteristics();

  machine->set_operating_system_build(GetOperatingSystemVersion());
  machine->set_browser_install_time_hours(GetInstallTimestamp().InHours());
  machine->set_utc_offset_ms(GetTimezoneOffset().InMilliseconds());
  machine->set_browser_language(
      content::GetContentClient()->browser()->GetApplicationLocale());
  machine->set_charset(charset_);
  machine->set_user_agent(content::GetContentClient()->GetUserAgent());
  machine->set_ram(base::SysInfo::AmountOfPhysicalMemory());
  machine->set_browser_build(chrome::VersionInfo().Version());
  AddFontsToFingerprint(*fonts_, machine);
  AddPluginsToFingerprint(plugins_, machine);
  AddAcceptLanguagesToFingerprint(accept_languages_, machine);
  AddScreenInfoToFingerprint(screen_info_, machine);
  AddCpuInfoToFingerprint(machine);
  AddGpuInfoToFingerprint(machine);

  // TODO(isherman): Store the partition size of the hard drives?

  Fingerprint_TransientState* transient_state =
      fingerprint->mutable_transient_state();
  Fingerprint_Dimension* inner_window_size =
      transient_state->mutable_inner_window_size();
  inner_window_size->set_width(content_bounds_.width());
  inner_window_size->set_height(content_bounds_.height());
  Fingerprint_Dimension* outer_window_size =
      transient_state->mutable_outer_window_size();
  outer_window_size->set_width(window_bounds_.width());
  outer_window_size->set_height(window_bounds_.height());

  // TODO(isherman): Record network performance data, which is theoretically
  // available to JS.

  // TODO(isherman): Record user behavior data.

  Fingerprint_Metadata* metadata = fingerprint->mutable_metadata();
  metadata->set_timestamp_ms(
      (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds());
  metadata->set_gaia_id(gaia_id_);
  metadata->set_fingerprinter_version(kFingerprinterVersion);

  callback_.Run(fingerprint.Pass());
}

}  // namespace

void GetFingerprint(
    int64 gaia_id,
    const gfx::Rect& window_bounds,
    const content::WebContents& web_contents,
    const PrefService& prefs,
    const base::Callback<void(scoped_ptr<Fingerprint>)>& callback) {
  gfx::Rect content_bounds;
  web_contents.GetView()->GetContainerBounds(&content_bounds);

  WebKit::WebScreenInfo screen_info;
  content::RenderWidgetHostView* host_view =
      web_contents.GetRenderWidgetHostView();
  if (host_view)
    host_view->GetRenderWidgetHost()->GetWebScreenInfo(&screen_info);

  internal::GetFingerprintInternal(
      gaia_id, window_bounds, content_bounds, screen_info, prefs, callback);
}

namespace internal {

void GetFingerprintInternal(
    int64 gaia_id,
    const gfx::Rect& window_bounds,
    const gfx::Rect& content_bounds,
    const WebKit::WebScreenInfo& screen_info,
    const PrefService& prefs,
    const base::Callback<void(scoped_ptr<Fingerprint>)>& callback) {
  // Begin loading all of the data that we need to load asynchronously.
  // This class is responsible for freeing its own memory.
  new FingerprintDataLoader(gaia_id, window_bounds, content_bounds, screen_info,
                            prefs, callback);
}

}  // namespace internal

}  // namespace risk
}  // namespace autofill
