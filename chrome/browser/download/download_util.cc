// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download utility implementation

#include "chrome/browser/download/download_util.h"

#include <string>

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/file_util.h"
#include "base/gfx/rect.h"
#include "base/i18n/time_formatting.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/time_format.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/mime_util.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkShader.h"

#if defined(TOOLKIT_VIEWS)
#include "app/os_exchange_data.h"
#include "views/drag_utils.h"
#endif

#if defined(TOOLKIT_VIEWS) && defined(OS_LINUX)
#include "app/drag_drop_types.h"
#include "views/widget/widget_gtk.h"
#endif

#if defined(OS_WIN)
#include "app/os_exchange_data_provider_win.h"
#include "base/base_drag_source.h"
#endif

namespace download_util {

// How many times to cycle the complete animation. This should be an odd number
// so that the animation ends faded out.
static const int kCompleteAnimationCycles = 5;

// Download opening ------------------------------------------------------------

bool CanOpenDownload(DownloadItem* download) {
  FilePath file_to_use = download->full_path();
  if (!download->original_name().value().empty())
    file_to_use = download->original_name();

  return !Extension::IsExtension(file_to_use) &&
         !download->manager()->IsExecutableFile(file_to_use);
}

void OpenDownload(DownloadItem* download) {
  if (download->state() == DownloadItem::IN_PROGRESS) {
    download->set_open_when_complete(!download->open_when_complete());
  } else if (download->state() == DownloadItem::COMPLETE) {
    download->NotifyObserversDownloadOpened();
    download->manager()->OpenDownload(download, NULL);
  }
}

// Download progress painting --------------------------------------------------

// Common bitmaps used for download progress animations. We load them once the
// first time we do a progress paint, then reuse them as they are always the
// same.
SkBitmap* g_foreground_16 = NULL;
SkBitmap* g_background_16 = NULL;
SkBitmap* g_foreground_32 = NULL;
SkBitmap* g_background_32 = NULL;

void PaintDownloadProgress(gfx::Canvas* canvas,
#if defined(TOOLKIT_VIEWS)
                           views::View* containing_view,
#endif
                           int origin_x,
                           int origin_y,
                           int start_angle,
                           int percent_done,
                           PaintDownloadProgressSize size) {
  // Load up our common bitmaps
  if (!g_background_16) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    g_foreground_16 = rb.GetBitmapNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_16);
    g_background_16 = rb.GetBitmapNamed(IDR_DOWNLOAD_PROGRESS_BACKGROUND_16);
    g_foreground_32 = rb.GetBitmapNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_32);
    g_background_32 = rb.GetBitmapNamed(IDR_DOWNLOAD_PROGRESS_BACKGROUND_32);
  }

  SkBitmap* background = (size == BIG) ? g_background_32 : g_background_16;
  SkBitmap* foreground = (size == BIG) ? g_foreground_32 : g_foreground_16;

  const int kProgressIconSize = (size == BIG) ? kBigProgressIconSize :
                                                kSmallProgressIconSize;

  // We start by storing the bounds of the background and foreground bitmaps
  // so that it is easy to mirror the bounds if the UI layout is RTL.
  gfx::Rect background_bounds(origin_x, origin_y,
                              background->width(), background->height());
  gfx::Rect foreground_bounds(origin_x, origin_y,
                              foreground->width(), foreground->height());

#if defined(TOOLKIT_VIEWS)
  // Mirror the positions if necessary.
  int mirrored_x = containing_view->MirroredLeftPointForRect(background_bounds);
  background_bounds.set_x(mirrored_x);
  mirrored_x = containing_view->MirroredLeftPointForRect(foreground_bounds);
  foreground_bounds.set_x(mirrored_x);
#endif

  // Draw the background progress image.
  SkPaint background_paint;
  canvas->DrawBitmapInt(*background,
                        background_bounds.x(),
                        background_bounds.y(),
                        background_paint);

  // Layer the foreground progress image in an arc proportional to the download
  // progress. The arc grows clockwise, starting in the midnight position, as
  // the download progresses. However, if the download does not have known total
  // size (the server didn't give us one), then we just spin an arc around until
  // we're done.
  float sweep_angle = 0.0;
  float start_pos = static_cast<float>(kStartAngleDegrees);
  if (percent_done < 0) {
    sweep_angle = kUnknownAngleDegrees;
    start_pos = static_cast<float>(start_angle);
  } else if (percent_done > 0) {
    sweep_angle = static_cast<float>(kMaxDegrees / 100.0 * percent_done);
  }

  // Set up an arc clipping region for the foreground image. Don't bother using
  // a clipping region if it would round to 360 (really 0) degrees, since that
  // would eliminate the foreground completely and be quite confusing (it would
  // look like 0% complete when it should be almost 100%).
  SkPaint foreground_paint;
  if (sweep_angle < static_cast<float>(kMaxDegrees - 1)) {
    SkRect oval;
    oval.set(SkIntToScalar(foreground_bounds.x()),
             SkIntToScalar(foreground_bounds.y()),
             SkIntToScalar(foreground_bounds.x() + kProgressIconSize),
             SkIntToScalar(foreground_bounds.y() + kProgressIconSize));
    SkPath path;
    path.arcTo(oval,
               SkFloatToScalar(start_pos),
               SkFloatToScalar(sweep_angle), false);
    path.lineTo(SkIntToScalar(foreground_bounds.x() + kProgressIconSize / 2),
                SkIntToScalar(foreground_bounds.y() + kProgressIconSize / 2));

    SkShader* shader =
        SkShader::CreateBitmapShader(*foreground,
                                     SkShader::kClamp_TileMode,
                                     SkShader::kClamp_TileMode);
    SkMatrix shader_scale;
    shader_scale.setTranslate(SkIntToScalar(foreground_bounds.x()),
                              SkIntToScalar(foreground_bounds.y()));
    shader->setLocalMatrix(shader_scale);
    foreground_paint.setShader(shader);
    foreground_paint.setAntiAlias(true);
    shader->unref();
    canvas->drawPath(path, foreground_paint);
    return;
  }

  canvas->DrawBitmapInt(*foreground,
                        foreground_bounds.x(),
                        foreground_bounds.y(),
                        foreground_paint);
}

void PaintDownloadComplete(gfx::Canvas* canvas,
#if defined(TOOLKIT_VIEWS)
                           views::View* containing_view,
#endif
                           int origin_x,
                           int origin_y,
                           double animation_progress,
                           PaintDownloadProgressSize size) {
  // Load up our common bitmaps.
  if (!g_foreground_16) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    g_foreground_16 = rb.GetBitmapNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_16);
    g_foreground_32 = rb.GetBitmapNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_32);
  }

  SkBitmap* complete = (size == BIG) ? g_foreground_32 : g_foreground_16;

  gfx::Rect complete_bounds(origin_x, origin_y,
                            complete->width(), complete->height());
#if defined(TOOLKIT_VIEWS)
  // Mirror the positions if necessary.
  complete_bounds.set_x(
      containing_view->MirroredLeftPointForRect(complete_bounds));
#endif

  // Start at full opacity, then loop back and forth five times before ending
  // at zero opacity.
  static const double PI = 3.141592653589793;
  double opacity = sin(animation_progress * PI * kCompleteAnimationCycles +
                   PI/2) / 2 + 0.5;

  SkRect bounds;
  bounds.set(SkIntToScalar(complete_bounds.x()),
             SkIntToScalar(complete_bounds.y()),
             SkIntToScalar(complete_bounds.x() + complete_bounds.width()),
             SkIntToScalar(complete_bounds.y() + complete_bounds.height()));
  canvas->saveLayerAlpha(&bounds,
                         static_cast<int>(255.0 * opacity),
                         SkCanvas::kARGB_ClipLayer_SaveFlag);
  canvas->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);
  canvas->DrawBitmapInt(*complete, complete_bounds.x(), complete_bounds.y());
  canvas->restore();
}

// Load a language dependent height so that the dangerous download confirmation
// message doesn't overlap with the download link label.
int GetBigProgressIconSize() {
  static int big_progress_icon_size = 0;
  if (big_progress_icon_size == 0) {
    string16 locale_size_str =
        WideToUTF16Hack(l10n_util::GetString(IDS_DOWNLOAD_BIG_PROGRESS_SIZE));
    bool rc = StringToInt(locale_size_str, &big_progress_icon_size);
    if (!rc || big_progress_icon_size < kBigProgressIconSize) {
      NOTREACHED();
      big_progress_icon_size = kBigProgressIconSize;
    }
  }

  return big_progress_icon_size;
}

int GetBigProgressIconOffset() {
  return (GetBigProgressIconSize() - kBigIconSize) / 2;
}

#if defined(TOOLKIT_VIEWS)
// Download dragging
void DragDownload(const DownloadItem* download,
                  SkBitmap* icon,
                  gfx::NativeView view) {
  DCHECK(download);

  // Set up our OLE machinery
  OSExchangeData data;

  if (icon) {
    drag_utils::CreateDragImageForFile(download->file_name().value(), icon,
                                       &data);
  }

  const FilePath full_path = download->full_path();
  data.SetFilename(full_path.ToWStringHack());

  std::string mime_type = download->mime_type();
  if (mime_type.empty())
    net::GetMimeTypeFromFile(full_path, &mime_type);

  // Add URL so that we can load supported files when dragged to TabContents.
  if (net::IsSupportedMimeType(mime_type)) {
    data.SetURL(GURL(WideToUTF8(full_path.ToWStringHack())),
                     download->file_name().ToWStringHack());
  }

#if defined(OS_WIN)
  scoped_refptr<BaseDragSource> drag_source(new BaseDragSource);

  // Run the drag and drop loop
  DWORD effects;
  DoDragDrop(OSExchangeDataProviderWin::GetIDataObject(data), drag_source.get(),
             DROPEFFECT_COPY | DROPEFFECT_LINK, &effects);
#elif defined(OS_LINUX)
  GtkWidget* root = gtk_widget_get_toplevel(view);
  if (!root)
    return;
  views::WidgetGtk* widget = views::WidgetGtk::GetViewForNative(root);
  if (!widget)
    return;

  widget->DoDrag(data, DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_LINK);
#endif  // OS_WIN
}
#elif defined(OS_LINUX)
void DragDownload(const DownloadItem* download,
                  SkBitmap* icon,
                  gfx::NativeView view) {
  NOTIMPLEMENTED();
}
#endif  // OS_LINUX

DictionaryValue* CreateDownloadItemValue(DownloadItem* download, int id) {
  DictionaryValue* file_value = new DictionaryValue();

  file_value->SetInteger(L"started",
    static_cast<int>(download->start_time().ToTimeT()));
  file_value->SetString(L"since_string",
    TimeFormat::RelativeDate(download->start_time(), NULL));
  file_value->SetString(L"date_string",
    base::TimeFormatShortDate(download->start_time()));
  file_value->SetInteger(L"id", id);
  file_value->SetString(L"file_path", download->full_path().ToWStringHack());
  // Keep file names as LTR.
  std::wstring file_name = download->GetFileName().ToWStringHack();
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    l10n_util::WrapStringWithLTRFormatting(&file_name);
  file_value->SetString(L"file_name", file_name);
  file_value->SetString(L"url", download->url().spec());

  if (download->state() == DownloadItem::IN_PROGRESS) {
    if (download->safety_state() == DownloadItem::DANGEROUS) {
      file_value->SetString(L"state", L"DANGEROUS");
    } else if (download->is_paused()) {
      file_value->SetString(L"state", L"PAUSED");
    } else {
      file_value->SetString(L"state", L"IN_PROGRESS");
    }

    file_value->SetString(L"progress_status_text",
       GetProgressStatusText(download));

    file_value->SetInteger(L"percent",
        static_cast<int>(download->PercentComplete()));
    file_value->SetInteger(L"received",
        static_cast<int>(download->received_bytes()));
  } else if (download->state() == DownloadItem::CANCELLED) {
    file_value->SetString(L"state", L"CANCELLED");
  } else if (download->state() == DownloadItem::COMPLETE) {
    if (download->safety_state() == DownloadItem::DANGEROUS) {
      file_value->SetString(L"state", L"DANGEROUS");
    } else {
      file_value->SetString(L"state", L"COMPLETE");
    }
  }

  file_value->SetInteger(L"total",
      static_cast<int>(download->total_bytes()));

  return file_value;
}

std::wstring GetProgressStatusText(DownloadItem* download) {
  int64 total = download->total_bytes();
  int64 size = download->received_bytes();
  DataUnits amount_units = GetByteDisplayUnits(size);
  std::wstring received_size = FormatBytes(size, amount_units, true);
  std::wstring amount = received_size;

  // Adjust both strings for the locale direction since we don't yet know which
  // string we'll end up using for constructing the final progress string.
  std::wstring amount_localized;
  if (l10n_util::AdjustStringForLocaleDirection(amount, &amount_localized)) {
    amount.assign(amount_localized);
    received_size.assign(amount_localized);
  }

  if (total) {
    amount_units = GetByteDisplayUnits(total);
    std::wstring total_text = FormatBytes(total, amount_units, true);
    std::wstring total_text_localized;
    if (l10n_util::AdjustStringForLocaleDirection(total_text,
                                                  &total_text_localized))
      total_text.assign(total_text_localized);

    amount = l10n_util::GetStringF(IDS_DOWNLOAD_TAB_PROGRESS_SIZE,
                                   received_size,
                                   total_text);
  } else {
    amount.assign(received_size);
  }
  amount_units = GetByteDisplayUnits(download->CurrentSpeed());
  std::wstring speed_text = FormatSpeed(download->CurrentSpeed(),
                                        amount_units, true);
  std::wstring speed_text_localized;
  if (l10n_util::AdjustStringForLocaleDirection(speed_text,
                                                &speed_text_localized))
    speed_text.assign(speed_text_localized);

  base::TimeDelta remaining;
  std::wstring time_remaining;
  if (download->is_paused())
    time_remaining = l10n_util::GetString(IDS_DOWNLOAD_PROGRESS_PAUSED);
  else if (download->TimeRemaining(&remaining))
    time_remaining = TimeFormat::TimeRemaining(remaining);

  if (time_remaining.empty()) {
    return l10n_util::GetStringF(IDS_DOWNLOAD_TAB_PROGRESS_STATUS_TIME_UNKNOWN,
                                 speed_text, amount);
  }
  return l10n_util::GetStringF(IDS_DOWNLOAD_TAB_PROGRESS_STATUS, speed_text,
                               amount, time_remaining);
}

#if !defined(OS_MACOSX)
void UpdateAppIconDownloadProgress(int download_count,
                                   bool progress_known,
                                   float progress) {
  // Win7 Superbar wants some pixel lovin! http://crbug.com/8039
}
#endif

}  // namespace download_util
