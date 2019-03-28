// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/arc_graphics_tracing/arc_graphics_tracing_handler.h"

#include <map>
#include <string>

#include "ash/public/cpp/shell_window_ids.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/tracing/arc_graphics_jank_detector.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_graphics_model.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/web_ui.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/codec/png_codec.h"

namespace chromeos {

namespace {

constexpr char kKeyIcon[] = "icon";
constexpr char kKeyTitle[] = "title";
constexpr char kKeyTasks[] = "tasks";

// Maximum interval to display.
constexpr base::TimeDelta kMaxIntervalToDisplay =
    base::TimeDelta::FromSecondsD(2.0);

std::unique_ptr<base::Value> BuildGraphicsModel(
    const std::string& data,
    base::DictionaryValue tasks_info,
    const base::TimeTicks& time_min,
    const base::TimeTicks& time_max) {
  arc::ArcTracingModel common_model;
  const base::TimeTicks time_min_clamped =
      std::max(time_min, time_max - kMaxIntervalToDisplay);
  common_model.SetMinMaxTime(
      (time_min_clamped - base::TimeTicks()).InMicroseconds(),
      (time_max - base::TimeTicks()).InMicroseconds());

  if (!common_model.Build(data)) {
    LOG(ERROR) << "Failed to build common model";
    return nullptr;
  }

  arc::ArcTracingGraphicsModel graphics_model;
  if (!graphics_model.Build(common_model)) {
    LOG(ERROR) << "Failed to build graphic buffers model";
    return nullptr;
  }

  std::unique_ptr<base::DictionaryValue> model = graphics_model.Serialize();
  model->SetKey(kKeyTasks, std::move(tasks_info));

  return model;
}

}  // namespace

ArcGraphicsTracingHandler::ArcGraphicsTracingHandler()
    : wm_helper_(exo::WMHelper::HasInstance() ? exo::WMHelper::GetInstance()
                                              : nullptr),
      weak_ptr_factory_(this) {
  DCHECK(wm_helper_);
}

ArcGraphicsTracingHandler::~ArcGraphicsTracingHandler() {
  if (tracing_active_)
    StopTracing();
}

void ArcGraphicsTracingHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "startTracing",
      base::BindRepeating(&ArcGraphicsTracingHandler::HandleStartTracing,
                          base::Unretained(this)));
}

void ArcGraphicsTracingHandler::OnWindowActivated(ActivationReason reason,
                                                  aura::Window* gained_active,
                                                  aura::Window* lost_active) {
  // Handle ARC current active window if any.
  DiscardActiveArcWindow();

  active_task_id_ =
      ArcAppWindowLauncherController::GetWindowTaskId(gained_active);
  if (active_task_id_ <= 0)
    return;

  arc_active_window_ = gained_active;
  arc_active_window_->AddObserver(this);
  // Limit tracing by newly activated window.
  tracing_time_min_ = TRACE_TIME_TICKS_NOW();
  jank_detector_ =
      std::make_unique<arc::ArcGraphicsJankDetector>(base::BindRepeating(
          &ArcGraphicsTracingHandler::OnJankDetected, base::Unretained(this)));

  UpdateActiveArcWindowInfo();

  exo::Surface* const surface = exo::GetShellMainSurface(arc_active_window_);
  DCHECK(surface);
  surface->SetCommitCallback(base::BindRepeating(
      &ArcGraphicsTracingHandler::OnCommit, weak_ptr_factory_.GetWeakPtr()));
}

void ArcGraphicsTracingHandler::OnCommit(exo::Surface* surface) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  jank_detector_->OnSample();
}

void ArcGraphicsTracingHandler::OnJankDetected(const base::Time& timestamp) {
  VLOG(1) << "Jank detected " << timestamp;
  StopTracing();
}

void ArcGraphicsTracingHandler::OnWindowPropertyChanged(aura::Window* window,
                                                        const void* key,
                                                        intptr_t old) {
  DCHECK_EQ(arc_active_window_, window);
  if (key != aura::client::kAppIconKey)
    return;

  UpdateActiveArcWindowInfo();
}

void ArcGraphicsTracingHandler::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(arc_active_window_, window);
  DiscardActiveArcWindow();
}

void ArcGraphicsTracingHandler::UpdateActiveArcWindowInfo() {
  DCHECK(arc_active_window_);
  base::DictionaryValue task_information;
  task_information.SetKey(kKeyTitle,
                          base::Value(arc_active_window_->GetTitle()));

  const gfx::ImageSkia* app_icon =
      arc_active_window_->GetProperty(aura::client::kAppIconKey);
  if (app_icon) {
    std::vector<unsigned char> png_data;
    if (gfx::PNGCodec::EncodeBGRASkBitmap(
            app_icon->GetRepresentation(1.0f).GetBitmap(),
            false /* discard_transparency */, &png_data)) {
      const std::string png_data_as_string(
          reinterpret_cast<const char*>(&png_data[0]), png_data.size());
      std::string icon_content;
      base::Base64Encode(png_data_as_string, &icon_content);
      task_information.SetKey(kKeyIcon, base::Value(icon_content));
    }
  }

  tasks_info_.SetKey(base::StringPrintf("%d", active_task_id_),
                     std::move(task_information));
}

void ArcGraphicsTracingHandler::DiscardActiveArcWindow() {
  if (!arc_active_window_)
    return;

  exo::Surface* const surface = exo::GetShellMainSurface(arc_active_window_);
  DCHECK(surface);
  surface->SetCommitCallback(exo::Surface::CommitCallback());

  arc_active_window_->RemoveObserver(this);
  jank_detector_.reset();
  arc_active_window_ = nullptr;
}

void ArcGraphicsTracingHandler::StartTracing(double duration) {
  base::trace_event::TraceConfig config(
      "-*,exo,viz,toplevel,gpu,cc,blink,disabled-by-default-android "
      "gfx,disabled-by-default-android hal",
      base::trace_event::RECORD_CONTINUOUSLY);
  tracing_active_ = true;
  config.EnableSystrace();
  content::TracingController::GetInstance()->StartTracing(
      config, base::BindOnce(&ArcGraphicsTracingHandler::OnTracingStarted,
                             weak_ptr_factory_.GetWeakPtr(), duration));
}

void ArcGraphicsTracingHandler::StopTracing() {
  tracing_active_ = false;
  DiscardActiveArcWindow();
  tracing_timer_.Stop();
  wm_helper_->RemoveActivationObserver(this);

  tracing_time_max_ = TRACE_TIME_TICKS_NOW();

  content::TracingController* const controller =
      content::TracingController::GetInstance();

  if (!controller->IsTracing())
    return;

  controller->StopTracing(content::TracingController::CreateStringEndpoint(
      base::BindRepeating(&ArcGraphicsTracingHandler::OnTracingStopped,
                          weak_ptr_factory_.GetWeakPtr())));
}

void ArcGraphicsTracingHandler::OnTracingStarted(double duration) {
  tasks_info_.Clear();

  aura::Window* const current_active = wm_helper_->GetActiveWindow();
  if (current_active) {
    OnWindowActivated(ActivationReason::ACTIVATION_CLIENT /* not used */,
                      current_active, nullptr);
  }
  wm_helper_->AddActivationObserver(this);

  tracing_time_min_ = TRACE_TIME_TICKS_NOW();

  tracing_timer_.Start(FROM_HERE, base::TimeDelta::FromSecondsD(duration),
                       base::BindOnce(&ArcGraphicsTracingHandler::StopTracing,
                                      base::Unretained(this)));
}

void ArcGraphicsTracingHandler::OnTracingStopped(
    std::unique_ptr<const base::DictionaryValue> metadata,
    base::RefCountedString* trace_data) {
  std::string string_data;
  string_data.swap(trace_data->data());

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&BuildGraphicsModel, std::move(string_data),
                     std::move(tasks_info_), tracing_time_min_,
                     tracing_time_max_),
      base::BindOnce(&ArcGraphicsTracingHandler::OnGraphicsModelReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcGraphicsTracingHandler::OnGraphicsModelReady(
    std::unique_ptr<base::Value> model) {
  if (!model)
    return;
  AllowJavascript();
  CallJavascriptFunction("cr.ArcGraphicsTracing.setModel", *model);
}

void ArcGraphicsTracingHandler::HandleStartTracing(
    const base::ListValue* args) {
  DCHECK_EQ(2U, args->GetSize());
  if ((!args->GetList()[0].is_double() && !args->GetList()[0].is_int()) ||
      (!args->GetList()[1].is_double() && !args->GetList()[1].is_int())) {
    LOG(ERROR) << "Invalid input";
    return;
  }
  const double delay = args->GetList()[0].GetDouble();
  const double duration = args->GetList()[1].GetDouble();
  tracing_timer_.Start(FROM_HERE, base::TimeDelta::FromSecondsD(delay),
                       base::BindOnce(&ArcGraphicsTracingHandler::StartTracing,
                                      base::Unretained(this), duration));
}

}  // namespace chromeos
