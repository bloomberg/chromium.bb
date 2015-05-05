// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webstore_widget_private/webstore_widget_private_api.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/api/webstore_widget_private.h"

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

  return RespondNow(Error("Not implemented"));
}

}  // namespace api
}  // namespace extensions
