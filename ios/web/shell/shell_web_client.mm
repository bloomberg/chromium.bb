// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/shell_web_client.h"

#import <UIKit/UIKit.h>

#include "base/memory/ptr_util.h"
#include "ios/web/public/user_agent.h"
#include "ios/web/shell/shell_web_main_parts.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

ShellWebClient::ShellWebClient() : web_main_parts_(nullptr) {}

ShellWebClient::~ShellWebClient() {
}

std::unique_ptr<web::WebMainParts> ShellWebClient::CreateWebMainParts() {
  auto web_main_parts = base::MakeUnique<ShellWebMainParts>();
  web_main_parts_ = web_main_parts.get();
  // TODO(crbug.com/703565): remove std::move() once Xcode 9.0+ is required.
  return std::move(web_main_parts);
}

ShellBrowserState* ShellWebClient::browser_state() const {
  return web_main_parts_->browser_state();
}

std::string ShellWebClient::GetProduct() const {
  return "CriOS/36.77.34.45";
}

std::string ShellWebClient::GetUserAgent(UserAgentType type) const {
  std::string product = GetProduct();
  return web::BuildUserAgentFromProduct(product);
}

void ShellWebClient::AllowCertificateError(
    WebState*,
    int /*cert_error*/,
    const net::SSLInfo&,
    const GURL&,
    bool overridable,
    const base::Callback<void(bool)>& callback) {
  base::Callback<void(bool)> block_callback(callback);
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:@"Your connection is not private"
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  [alert addAction:[UIAlertAction actionWithTitle:@"Go Back"
                                            style:UIAlertActionStyleCancel
                                          handler:^(UIAlertAction*) {
                                            block_callback.Run(false);
                                          }]];

  if (overridable) {
    [alert addAction:[UIAlertAction actionWithTitle:@"Continue"
                                              style:UIAlertActionStyleDefault
                                            handler:^(UIAlertAction*) {
                                              block_callback.Run(true);
                                            }]];
  }
  [[UIApplication sharedApplication].keyWindow.rootViewController
      presentViewController:alert
                   animated:YES
                 completion:nil];
}

}  // namespace web
