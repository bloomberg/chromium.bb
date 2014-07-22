// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/notifications_native_handler.h"

#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/common/extensions/api/notifications/notification_style.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "content/public/renderer/v8_value_converter.h"
#include "ui/base/layout.h"

namespace extensions {

NotificationsNativeHandler::NotificationsNativeHandler(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction(
      "GetNotificationImageSizes",
      base::Bind(&NotificationsNativeHandler::GetNotificationImageSizes,
                 base::Unretained(this)));
}

void NotificationsNativeHandler::GetNotificationImageSizes(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  NotificationBitmapSizes bitmap_sizes = GetNotificationBitmapSizes();

  float scale_factor =
      ui::GetScaleForScaleFactor(ui::GetSupportedScaleFactors().back());

  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetDouble("scaleFactor", scale_factor);
  dict->SetInteger("icon.width", bitmap_sizes.icon_size.width());
  dict->SetInteger("icon.height", bitmap_sizes.icon_size.height());
  dict->SetInteger("image.width", bitmap_sizes.image_size.width());
  dict->SetInteger("image.height", bitmap_sizes.image_size.height());
  dict->SetInteger("buttonIcon.width", bitmap_sizes.button_icon_size.width());
  dict->SetInteger("buttonIcon.height", bitmap_sizes.button_icon_size.height());
  dict->SetInteger("appIconMask.width",
                   bitmap_sizes.app_icon_mask_size.width());
  dict->SetInteger("appIconMask.height",
                   bitmap_sizes.app_icon_mask_size.height());

  scoped_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());
  args.GetReturnValue().Set(
      converter->ToV8Value(dict.get(), context()->v8_context()));
}

}  // namespace extensions
