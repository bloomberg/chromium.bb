// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_conversion_helper.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/api/notification_provider.h"
#include "chrome/common/extensions/api/notifications/notification_style.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/skia_util.h"

void NotificationConversionHelper::NotificationToNotificationOptions(
    const Notification& notification,
    extensions::api::notifications::NotificationOptions* options) {
  // Extract required fields: type, title, message, and icon.
  std::string type = MapTypeToString(notification.type());
  options->type = extensions::api::notifications::ParseTemplateType(type);

  if (!notification.icon().IsEmpty()) {
    scoped_ptr<extensions::api::notifications::NotificationBitmap> icon(
        new extensions::api::notifications::NotificationBitmap());
    GfxImageToNotificationBitmap(&notification.icon(), icon.get());
    options->icon_bitmap = icon.Pass();
  }

  options->title.reset(
      new std::string(base::UTF16ToUTF8(notification.title())));
  options->message.reset(
      new std::string(base::UTF16ToUTF8(notification.message())));

  // Handle optional data provided.
  const message_center::RichNotificationData* rich_data =
      &notification.rich_notification_data();

  if (!rich_data->small_image.IsEmpty()) {
    scoped_ptr<extensions::api::notifications::NotificationBitmap> icon_mask(
        new extensions::api::notifications::NotificationBitmap());
    GfxImageToNotificationBitmap(&rich_data->small_image, icon_mask.get());
    options->app_icon_mask_bitmap = icon_mask.Pass();
  }

  options->priority.reset(new int(rich_data->priority));

  options->is_clickable.reset(new bool(rich_data->clickable));

  options->event_time.reset(new double(rich_data->timestamp.ToDoubleT()));

  if (!rich_data->context_message.empty())
    options->context_message.reset(
        new std::string(base::UTF16ToUTF8(rich_data->context_message)));

  if (!rich_data->buttons.empty()) {
    scoped_ptr<std::vector<
        linked_ptr<extensions::api::notifications::NotificationButton> > >
        button_list(new std::vector<
            linked_ptr<extensions::api::notifications::NotificationButton> >);
    for (size_t i = 0; i < rich_data->buttons.size(); i++) {
      linked_ptr<extensions::api::notifications::NotificationButton> button(
          new extensions::api::notifications::NotificationButton);
      button->title = base::UTF16ToUTF8(rich_data->buttons[i].title);

      if (!rich_data->buttons[i].icon.IsEmpty()) {
        scoped_ptr<extensions::api::notifications::NotificationBitmap> icon(
            new extensions::api::notifications::NotificationBitmap());
        GfxImageToNotificationBitmap(&rich_data->buttons[i].icon, icon.get());
        button->icon_bitmap = icon.Pass();
      }
      button_list->push_back(button);
    }
    options->buttons = button_list.Pass();
  }

  // Only image type notifications should have images.
  if (type == "image" && !rich_data->image.IsEmpty()) {
    scoped_ptr<extensions::api::notifications::NotificationBitmap> image(
        new extensions::api::notifications::NotificationBitmap());
    GfxImageToNotificationBitmap(&notification.image(), image.get());
    options->image_bitmap = image.Pass();
  } else if (type != "image" && !rich_data->image.IsEmpty()) {
    DVLOG(1) << "Only image type notifications should have images.";
  }

  // Only progress type notifications should have progress bars.
  if (type == "progress")
    options->progress.reset(new int(rich_data->progress));
  else if (rich_data->progress != 0)
    DVLOG(1) << "Only progress type notifications should have progress.";

  // Only list type notifications should have lists.
  if (type == "list" && !rich_data->items.empty()) {
    scoped_ptr<std::vector<
        linked_ptr<extensions::api::notifications::NotificationItem> > >
        list(new std::vector<
            linked_ptr<extensions::api::notifications::NotificationItem> >);
    for (size_t j = 0; j < rich_data->items.size(); j++) {
      linked_ptr<extensions::api::notifications::NotificationItem> item(
          new extensions::api::notifications::NotificationItem);
      item->title = base::UTF16ToUTF8(rich_data->items[j].title);
      item->message = base::UTF16ToUTF8(rich_data->items[j].message);
      list->push_back(item);
    }
    options->items = list.Pass();
  } else if (type != "list" && !rich_data->items.empty()) {
    DVLOG(1) << "Only list type notifications should have lists.";
  }
}

void NotificationConversionHelper::GfxImageToNotificationBitmap(
    const gfx::Image* gfx_image,
    extensions::api::notifications::NotificationBitmap* notification_bitmap) {
  SkBitmap sk_bitmap = gfx_image->AsBitmap();
  sk_bitmap.lockPixels();

  notification_bitmap->width = sk_bitmap.width();
  notification_bitmap->height = sk_bitmap.height();
  int pixel_count = sk_bitmap.width() * sk_bitmap.height();
  const int BYTES_PER_PIXEL = 4;

  uint32_t* bitmap_pixels = sk_bitmap.getAddr32(0, 0);
  const unsigned char* bitmap =
      reinterpret_cast<const unsigned char*>(bitmap_pixels);
  scoped_ptr<unsigned char[]> rgba_bitmap_data(
      new unsigned char[pixel_count * BYTES_PER_PIXEL]);

  gfx::ConvertSkiaToRGBA(bitmap, pixel_count, rgba_bitmap_data.get());
  sk_bitmap.unlockPixels();

  notification_bitmap->data.reset(new std::string(
      rgba_bitmap_data.get(),
      (rgba_bitmap_data.get() + pixel_count * BYTES_PER_PIXEL)));
  return;
}

bool NotificationConversionHelper::NotificationBitmapToGfxImage(
    float max_scale,
    const gfx::Size& target_size_dips,
    extensions::api::notifications::NotificationBitmap* notification_bitmap,
    gfx::Image* return_image) {
  if (!notification_bitmap)
    return false;

  const int max_device_pixel_width = target_size_dips.width() * max_scale;
  const int max_device_pixel_height = target_size_dips.height() * max_scale;

  const int BYTES_PER_PIXEL = 4;

  const int width = notification_bitmap->width;
  const int height = notification_bitmap->height;

  if (width < 0 || height < 0 || width > max_device_pixel_width ||
      height > max_device_pixel_height)
    return false;

  // Ensure we have rgba data.
  std::string* rgba_data = notification_bitmap->data.get();
  if (!rgba_data)
    return false;

  const size_t rgba_data_length = rgba_data->length();
  const size_t rgba_area = width * height;

  if (rgba_data_length != rgba_area * BYTES_PER_PIXEL)
    return false;

  SkBitmap bitmap;
  // Allocate the actual backing store with the sanitized dimensions.
  if (!bitmap.tryAllocN32Pixels(width, height))
    return false;

  // Ensure that our bitmap and our data now refer to the same number of pixels.
  if (rgba_data_length != bitmap.getSafeSize())
    return false;

  uint32_t* pixels = bitmap.getAddr32(0, 0);
  const char* c_rgba_data = rgba_data->data();

  for (size_t t = 0; t < rgba_area; ++t) {
    // |c_rgba_data| is RGBA, pixels is ARGB.
    size_t rgba_index = t * BYTES_PER_PIXEL;
    pixels[t] =
        SkPreMultiplyColor(((c_rgba_data[rgba_index + 3] & 0xFF) << 24) |
                           ((c_rgba_data[rgba_index + 0] & 0xFF) << 16) |
                           ((c_rgba_data[rgba_index + 1] & 0xFF) << 8) |
                           ((c_rgba_data[rgba_index + 2] & 0xFF) << 0));
  }

  // TODO(dewittj): Handle HiDPI images with more than one scale factor
  // representation.
  gfx::ImageSkia skia(gfx::ImageSkiaRep(bitmap, 1.0f));
  *return_image = gfx::Image(skia);
  return true;
}

std::string NotificationConversionHelper::MapTypeToString(
    message_center::NotificationType type) {
  switch (type) {
    case message_center::NOTIFICATION_TYPE_BASE_FORMAT:
      return "basic";
    case message_center::NOTIFICATION_TYPE_IMAGE:
      return "image";
    case message_center::NOTIFICATION_TYPE_MULTIPLE:
      return "list";
    case message_center::NOTIFICATION_TYPE_PROGRESS:
      return "progress";
    default:
      NOTREACHED();
      return "";
  }
}
