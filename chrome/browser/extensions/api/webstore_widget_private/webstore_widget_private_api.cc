// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webstore_widget_private/webstore_widget_private_api.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/extensions/api/webstore_widget_private/app_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/webstore_widget_private.h"

namespace {
const char kGoogleCastApiExtensionId[] = "mafeflapfdfljijmlienjedomfjfmhpd";
}  // namespace

namespace extensions {
namespace api {

WebstoreWidgetPrivateGetStringsFunction::
    WebstoreWidgetPrivateGetStringsFunction() {
}

WebstoreWidgetPrivateGetStringsFunction::
    ~WebstoreWidgetPrivateGetStringsFunction() {
}

ExtensionFunction::ResponseAction
WebstoreWidgetPrivateGetStringsFunction::Run() {
  base::DictionaryValue* dict = new base::DictionaryValue();
  return RespondNow(OneArgument(dict));
}

WebstoreWidgetPrivateInstallWebstoreItemFunction::
    WebstoreWidgetPrivateInstallWebstoreItemFunction() {
}

WebstoreWidgetPrivateInstallWebstoreItemFunction::
    ~WebstoreWidgetPrivateInstallWebstoreItemFunction() {
}

ExtensionFunction::ResponseAction
WebstoreWidgetPrivateInstallWebstoreItemFunction::Run() {
  const scoped_ptr<webstore_widget_private::InstallWebstoreItem::Params> params(
      webstore_widget_private::InstallWebstoreItem::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->item_id.empty())
    return RespondNow(Error("App ID empty."));

  bool allow_silent_install =
      extension()->id() == file_manager::kVideoPlayerAppId &&
      params->item_id == kGoogleCastApiExtensionId;
  if (params->silent_installation && !allow_silent_install)
    return RespondNow(Error("Silent installation not allowed."));

  const extensions::WebstoreStandaloneInstaller::Callback callback = base::Bind(
      &WebstoreWidgetPrivateInstallWebstoreItemFunction::OnInstallComplete,
      this);

  scoped_refptr<webstore_widget::AppInstaller> installer(
      new webstore_widget::AppInstaller(
          GetAssociatedWebContents(), params->item_id,
          Profile::FromBrowserContext(browser_context()),
          params->silent_installation, callback));
  // installer will be AddRef()'d in BeginInstall().
  installer->BeginInstall();

  return RespondLater();
}

void WebstoreWidgetPrivateInstallWebstoreItemFunction::OnInstallComplete(
    bool success,
    const std::string& error,
    extensions::webstore_install::Result result) {
  if (!success)
    SetError(error);

  SendResponse(success);
}

}  // namespace api
}  // namespace extensions
