// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_

#include "base/containers/flat_set.h"
#include "chrome/browser/devtools/protocol/browser.h"

class Profile;

class BrowserHandler : public protocol::Browser::Backend {
 public:
  BrowserHandler(protocol::UberDispatcher* dispatcher,
                 const std::string& target_id);
  ~BrowserHandler() override;

  // Browser::Backend:
  protocol::Response GetWindowForTarget(
      protocol::Maybe<std::string> target_id,
      int* out_window_id,
      std::unique_ptr<protocol::Browser::Bounds>* out_bounds) override;
  protocol::Response GetWindowBounds(
      int window_id,
      std::unique_ptr<protocol::Browser::Bounds>* out_bounds) override;
  protocol::Response Close() override;
  protocol::Response SetWindowBounds(
      int window_id,
      std::unique_ptr<protocol::Browser::Bounds> out_bounds) override;
  protocol::Response Disable() override;
  protocol::Response GrantPermissions(
      const std::string& origin,
      std::unique_ptr<protocol::Array<protocol::Browser::PermissionType>>
          permissions,
      protocol::Maybe<std::string> browser_context_id) override;
  protocol::Response ResetPermissions(
      protocol::Maybe<std::string> browser_context_id) override;
  protocol::Response SetDockTile(
      protocol::Maybe<std::string> label,
      protocol::Maybe<protocol::Binary> image) override;

 private:
  protocol::Response FindProfile(
      const protocol::Maybe<std::string>& browser_context_id,
      Profile** profile);

  base::flat_set<std::string> contexts_with_overridden_permissions_;
  std::string target_id_;

  DISALLOW_COPY_AND_ASSIGN(BrowserHandler);
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_
