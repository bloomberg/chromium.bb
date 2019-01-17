// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/browser/webrunner_net_log.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/values.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log_util.h"

namespace webrunner {

namespace {

std::unique_ptr<base::DictionaryValue> GetWebRunnerConstants() {
  std::unique_ptr<base::DictionaryValue> constants_dict =
      net::GetNetConstants();

  base::DictionaryValue dict;
  dict.SetKey("name", base::Value("webrunner"));
  dict.SetKey(
      "command_line",
      base::Value(
          base::CommandLine::ForCurrentProcess()->GetCommandLineString()));

  constants_dict->SetKey("clientInfo", std::move(dict));

  return constants_dict;
}

}  // namespace

WebRunnerNetLog::WebRunnerNetLog(const base::FilePath& log_path) {
  if (!log_path.empty()) {
    net::NetLogCaptureMode capture_mode = net::NetLogCaptureMode::Default();
    file_net_log_observer_ = net::FileNetLogObserver::CreateUnbounded(
        log_path, GetWebRunnerConstants());
    file_net_log_observer_->StartObserving(this, capture_mode);
  }
}

WebRunnerNetLog::~WebRunnerNetLog() {
  if (file_net_log_observer_)
    file_net_log_observer_->StopObserving(nullptr, base::OnceClosure());
}

}  // namespace webrunner
