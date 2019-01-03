// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/contained_shell_client.h"

#include <utility>

#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/window_pin_type.mojom.h"
#include "base/command_line.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/constants/chromeos_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {
// TODO(b/119418627): implement the contained shell experience and put its URL
// here.
constexpr char kDefaultContainedShellUrl[] = "about:blank";
}  // namespace

ContainedShellClient::ContainedShellClient() {
  ash::mojom::ContainedShellControllerPtr contained_shell_controller;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &contained_shell_controller);

  ash::mojom::ContainedShellClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  contained_shell_controller->SetClient(std::move(client));
}

ContainedShellClient::~ContainedShellClient() = default;

void ContainedShellClient::LaunchContainedShell(const AccountId& account_id) {
  auto* web_view = new views::WebView(
      chromeos::ProfileHelper::Get()->GetProfileByAccountId(account_id));
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);

  auto* widget = new views::Widget;
  widget->Init(params);
  widget->SetContentsView(web_view);

  const std::string url_flag =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          chromeos::switches::kContainedShellUrl);

  if (!url_flag.empty())
    web_view->LoadInitialURL(GURL(url_flag));
  else
    web_view->LoadInitialURL(GURL(kDefaultContainedShellUrl));

  widget->SetFullscreen(true);
  widget->Show();
}
