// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/client_hints.h"

#include <locale>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/screen.h"

namespace {

float FetchScreenInfoOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // TODO(bengr): Consider multi-display scenarios.
  gfx::Display display = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  return display.device_scale_factor();
}

}  // namespace

const char ClientHints::kDevicePixelRatioHeader[] = "CH-DPR";

ClientHints::ClientHints()
    : weak_ptr_factory_(this) {
}

ClientHints::~ClientHints() {
}

void ClientHints::Init() {
  // TODO(bengr): Observe the device pixel ratio in case it changes during
  // the Chrome session.
  RetrieveScreenInfo();
}

bool ClientHints::RetrieveScreenInfo() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&FetchScreenInfoOnUIThread),
      base::Bind(
          &ClientHints::UpdateScreenInfo, weak_ptr_factory_.GetWeakPtr()));
}

const std::string& ClientHints::GetDevicePixelRatioHeader() const {
  return screen_hints_;
}

void ClientHints::UpdateScreenInfo(float device_pixel_ratio_value) {
  if (device_pixel_ratio_value <= 0.0)
    return;
  std::string device_pixel_ratio = base::StringPrintf("%.2f",
      device_pixel_ratio_value);
  // Make sure the Client Hints value doesn't change
  // according to the machine's locale
  std::locale locale;
  for (std::string::iterator it = device_pixel_ratio.begin();
       it != device_pixel_ratio.end(); ++it) {
    if (!std::isdigit(*it, locale))
      *it = '.';
  }
  screen_hints_ = device_pixel_ratio;
}

