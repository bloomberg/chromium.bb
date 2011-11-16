// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains code for handling "about:" URLs in the browser process.

#ifndef CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H_
#define CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/process.h"
#include "base/stringprintf.h"
#include "build/build_config.h"  // USE_TCMALLOC

template <typename T> struct DefaultSingletonTraits;
class GURL;

namespace content {
class BrowserContext;
}

// Register a data source for a known source name. Safe to call multiple times.
// |name| may be an unkown host (e.g. "chrome://foo/"); only handle known hosts.
// In general case WillHandleBrowserAboutURL will initialize all data sources.
// But in some case like navigating to chrome://oobe on boot and loading
// chrome://terms in an iframe there, kChromeUITermsHost data source needs to
// be initialized separately.
void InitializeAboutDataSource(const std::string& name,
                               content::BrowserContext* browser_context);

// Returns true if the given URL will be handled by the browser about handler.
// |url| should have been processed by URLFixerUpper::FixupURL, which replaces
// the about: scheme with chrome:// for all about:foo URLs except "about:blank".
// Some |url| host values will be replaced with their respective redirects.
//
// This is used by BrowserURLHandler.
bool WillHandleBrowserAboutURL(GURL* url,
                               content::BrowserContext* browser_context);

// We have a few magic commands that don't cause navigations, but rather pop up
// dialogs. This function handles those cases, and returns true if so. In this
// case, normal tab navigation should be skipped.
bool HandleNonNavigationAboutURL(const GURL& url);

// Gets the paths that are shown in chrome://chrome-urls.
std::vector<std::string> ChromePaths();

#if defined(USE_TCMALLOC)
// A map of header strings (e.g. "Browser", "Renderer PID 123")
// to the tcmalloc output collected for each process.
typedef std::map<std::string, std::string> AboutTcmallocOutputsType;

class AboutTcmallocOutputs {
 public:
  // Returns the singleton instance.
  static AboutTcmallocOutputs* GetInstance();

  AboutTcmallocOutputsType* outputs() { return &outputs_; }

  // Records the output for a specified header string.
  void SetOutput(const std::string& header, const std::string& output) {
    outputs_[header] = output;
  }

  // Callback for output returned from renderer processes.  Adds
  // the output for a canonical renderer header string that
  // incorporates the pid.
  void RendererCallback(base::ProcessId pid, const std::string& output) {
    SetOutput(
        base::StringPrintf("Renderer PID %d", static_cast<int>(pid)), output);
  }

 private:
  AboutTcmallocOutputs();
  ~AboutTcmallocOutputs();

  AboutTcmallocOutputsType outputs_;

  friend struct DefaultSingletonTraits<AboutTcmallocOutputs>;

  DISALLOW_COPY_AND_ASSIGN(AboutTcmallocOutputs);
};

// Glue between the callback task and the method in the singleton.
void AboutTcmallocRendererCallback(base::ProcessId pid,
                                   const std::string& output);
#endif

#endif  // CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H_
