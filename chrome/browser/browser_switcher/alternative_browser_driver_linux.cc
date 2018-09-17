// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/alternative_browser_driver.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_switcher/alternative_browser_launcher.h"
#include "url/gurl.h"

namespace browser_switcher {

namespace {

const char kUrlVarName[] = "${url}";

const char kChromeExecutableName[] = "google-chrome";
const char kFirefoxExecutableName[] = "firefox";
const char kOperaExecutableName[] = "opera";

const char kChromeVarName[] = "${chrome}";
const char kFirefoxVarName[] = "${firefox}";
const char kOperaVarName[] = "${opera}";

const struct {
  const char* var_name;
  const char* executable_name;
} kBrowserVarMappings[] = {
    {kChromeVarName, kChromeExecutableName},
    {kFirefoxVarName, kFirefoxExecutableName},
    {kOperaVarName, kOperaExecutableName},
};

}  // namespace

AlternativeBrowserDriver::~AlternativeBrowserDriver() {}

AlternativeBrowserDriverImpl::AlternativeBrowserDriverImpl() {}
AlternativeBrowserDriverImpl::~AlternativeBrowserDriverImpl() {}

void AlternativeBrowserDriverImpl::SetBrowserPath(base::StringPiece path) {
  browser_path_ = path.as_string();
  for (const auto& mapping : kBrowserVarMappings) {
    if (!browser_path_.compare(mapping.var_name)) {
      browser_path_ = mapping.executable_name;
    }
  }
}

void AlternativeBrowserDriverImpl::SetBrowserParameters(
    const base::ListValue* parameters) {
  browser_params_.clear();
  browser_params_.reserve(parameters->GetList().size());
  for (const auto& param : *parameters) {
    DCHECK(param.is_string());
    browser_params_.push_back(param.GetString());
  }
}

bool AlternativeBrowserDriverImpl::TryLaunch(const GURL& url) {
  if (browser_path_.empty()) {
    LOG(ERROR) << "Alternative browser not configured. "
               << "Aborting browser switch.";
    return false;
  }

  VLOG(2) << "Launching alternative browser...";
  VLOG(2) << "  path = " << browser_path_;
  VLOG(2) << "  url = " << url.spec();

  CHECK(url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile());

  const int max_num_args = browser_params_.size() + 2;
  std::vector<std::string> argv;
  argv.reserve(max_num_args);
  argv.push_back(browser_path_);
  AppendCommandLineArguments(&argv, browser_params_, url);

  base::CommandLine cmd_line = base::CommandLine(argv);
  base::LaunchOptions options;
  // Don't close the alternative browser when Chrome exits.
  options.new_process_group = true;
  if (!base::LaunchProcess(cmd_line, options).IsValid()) {
    LOG(ERROR) << "Could not start the alternative browser!";
    return false;
  }
  return true;
}

// static
void AlternativeBrowserDriverImpl::AppendCommandLineArguments(
    std::vector<std::string>* argv,
    const std::vector<std::string>& raw_args,
    const GURL& url) {
  // TODO(crbug/882520): Do environment variable and tilde expansion.
  bool contains_url = false;
  for (const auto& arg : raw_args) {
    size_t url_index = arg.find(kUrlVarName);
    if (url_index != std::string::npos) {
      std::string expanded_arg = arg;
      expanded_arg.replace(url_index, strlen(kUrlVarName), url.spec());
      argv->push_back(expanded_arg);
      contains_url = true;
    } else {
      argv->push_back(arg);
    }
  }
  if (!contains_url)
    argv->push_back(url.spec());
}

}  // namespace browser_switcher
