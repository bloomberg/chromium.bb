// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_function.h"

#include <utility>

#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/extension_function_dispatcher.h"

using content::RenderViewHost;
using content::WebContents;

ChromeUIThreadExtensionFunction::ChromeUIThreadExtensionFunction()
    : chrome_details_(this) {}

Profile* ChromeUIThreadExtensionFunction::GetProfile() const {
  return Profile::FromBrowserContext(context_);
}

// TODO(stevenjb): Replace this with GetExtensionWindowController().
Browser* ChromeUIThreadExtensionFunction::GetCurrentBrowser() {
  return chrome_details_.GetCurrentBrowser();
}

extensions::WindowController*
ChromeUIThreadExtensionFunction::GetExtensionWindowController() {
  return chrome_details_.GetExtensionWindowController();
}

void ChromeUIThreadExtensionFunction::SetError(const std::string& error) {
  error_ = error;
}

content::WebContents*
ChromeUIThreadExtensionFunction::GetAssociatedWebContents() {
  return chrome_details_.GetAssociatedWebContents();
}

const std::string& ChromeUIThreadExtensionFunction::GetError() const {
  return error_.empty() ? UIThreadExtensionFunction::GetError() : error_;
}

void ChromeUIThreadExtensionFunction::SendResponse(bool success) {
  ResponseValue response;
  if (success) {
    response = ArgumentList(std::move(results_));
  } else {
    response = results_ ? ErrorWithArguments(std::move(results_), error_)
                        : Error(error_);
  }
  Respond(std::move(response));
}

void ChromeUIThreadExtensionFunction::SetResult(
    std::unique_ptr<base::Value> result) {
  results_.reset(new base::ListValue());
  results_->Append(std::move(result));
}

void ChromeUIThreadExtensionFunction::SetResultList(
    std::unique_ptr<base::ListValue> results) {
  results_ = std::move(results);
}

ChromeUIThreadExtensionFunction::~ChromeUIThreadExtensionFunction() {
}

ChromeAsyncExtensionFunction::ChromeAsyncExtensionFunction() {
}

ChromeAsyncExtensionFunction::~ChromeAsyncExtensionFunction() {}

ExtensionFunction::ResponseAction ChromeAsyncExtensionFunction::Run() {
  if (RunAsync())
    return RespondLater();
  // TODO(devlin): Track these down and eliminate them if possible. We
  // shouldn't return results and an error.
  if (results_)
    return RespondNow(ErrorWithArguments(std::move(results_), error_));
  return RespondNow(Error(error_));
}

// static
bool ChromeAsyncExtensionFunction::ValidationFailure(
    ChromeAsyncExtensionFunction* function) {
  return false;
}
