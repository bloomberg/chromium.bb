// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/arc_graphics_tracing/arc_graphics_tracing_handler.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_graphics_model.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_model.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

namespace {

std::unique_ptr<base::Value> BuildGraphicsModel(const std::string& data) {
  arc::ArcTracingModel common_model;
  if (!common_model.Build(data)) {
    LOG(ERROR) << "Failed to build common model";
    return nullptr;
  }

  arc::ArcTracingGraphicsModel graphics_model;
  if (!graphics_model.Build(common_model)) {
    LOG(ERROR) << "Failed to build graphic buffers model";
    return nullptr;
  }

  return graphics_model.Serialize();
}

}  // namespace

ArcGraphicsTracingHandler::ArcGraphicsTracingHandler()
    : weak_ptr_factory_(this) {}

ArcGraphicsTracingHandler::~ArcGraphicsTracingHandler() = default;

void ArcGraphicsTracingHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "startTracing",
      base::BindRepeating(&ArcGraphicsTracingHandler::HandleStartTracing,
                          base::Unretained(this)));
}

void ArcGraphicsTracingHandler::StartTracing(double duration) {
  base::trace_event::TraceConfig config(
      "-*,exo,viz,toplevel,gpu,cc,blink,disabled-by-default-android "
      "gfx,disabled-by-default-android hal",
      base::trace_event::RECORD_CONTINUOUSLY);
  config.EnableSystrace();
  content::TracingController::GetInstance()->StartTracing(
      config, base::BindOnce(&ArcGraphicsTracingHandler::OnTracingStarted,
                             weak_ptr_factory_.GetWeakPtr(), duration));
}

void ArcGraphicsTracingHandler::StopTracing() {
  content::TracingController::GetInstance()->StopTracing(
      content::TracingController::CreateStringEndpoint(
          base::BindRepeating(&ArcGraphicsTracingHandler::OnTracingStopped,
                              weak_ptr_factory_.GetWeakPtr())));
}

void ArcGraphicsTracingHandler::OnTracingStarted(double duration) {
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
      base::BindOnce(&BuildGraphicsModel, std::move(string_data)),
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
