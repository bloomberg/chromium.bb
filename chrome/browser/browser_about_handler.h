// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains code for handling "about:" URLs in the browser process.

#ifndef CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H_
#define CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H_
#pragma once

#include <map>
#include <string>

#include "base/process.h"
#include "base/string_util.h"

template <typename T> struct DefaultSingletonTraits;
class GURL;
class Profile;

// Decides whether the given URL will be handled by the browser about handler
// and returns true if so. On true, it may also modify the given URL to be the
// final form (we fix up most "about:" URLs to be "chrome:" because WebKit
// handles all "about:" URLs as "about:blank.
//
// This is used by BrowserURLHandler.
bool WillHandleBrowserAboutURL(GURL* url, Profile* profile);

// Register the data source for chrome://about URLs.
// Safe to call multiple times.
void InitializeAboutDataSource(Profile* profile);

// We have a few magic commands that don't cause navigations, but rather pop up
// dialogs. This function handles those cases, and returns true if so. In this
// case, normal tab navigation should be skipped.
bool HandleNonNavigationAboutURL(const GURL& url);

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
  void SetOutput(std::string header, std::string output) {
    outputs_[header] = output;
  }

  // Callback for output returned from renderer processes.  Adds
  // the output for a canonical renderer header string that
  // incorporates the pid.
  void RendererCallback(base::ProcessId pid, std::string output) {
    SetOutput(StringPrintf("Renderer PID %d", static_cast<int>(pid)), output);
  }

 private:
  AboutTcmallocOutputs();
  ~AboutTcmallocOutputs();

  AboutTcmallocOutputsType outputs_;

  friend struct DefaultSingletonTraits<AboutTcmallocOutputs>;

  DISALLOW_COPY_AND_ASSIGN(AboutTcmallocOutputs);
};

// Glue between the callback task and the method in the singleton.
void AboutTcmallocRendererCallback(base::ProcessId pid, std::string output);
#endif

#endif  // CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H_
