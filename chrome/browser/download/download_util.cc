// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download utility implementation

#include "chrome/browser/download/download_util.h"

#if defined(OS_WIN)
#include <shobjidl.h>
#endif
#include <string>

#include "base/file_util.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "base/win/windows_version.h"
#include "chrome/browser/download/download_extensions.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/time_format.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/download/download_state_info.h"
#include "content/browser/download/download_types.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/base/dragdrop/os_exchange_data.h"
#include "views/drag_utils.h"
#endif

#if defined(TOOLKIT_USES_GTK)
#if defined(TOOLKIT_VIEWS)
#include "ui/base/dragdrop/drag_drop_types.h"
#include "views/widget/native_widget_gtk.h"
#elif defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/custom_drag.h"
#include "chrome/browser/ui/gtk/unity_service.h"
#endif  // defined(TOOLKIT_GTK)
#endif  // defined(TOOLKIT_USES_GTK)

#if defined(OS_WIN) && !defined(USE_AURA)
#include "base/win/scoped_comptr.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/dragdrop/drag_source.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#endif

// TODO(phajdan.jr): Find some standard location for this, maintaining
// the same value on all platforms.
static const double PI = 3.141592653589793;

namespace download_util {

// How many times to cycle the complete animation. This should be an odd number
// so that the animation ends faded out.
static const int kCompleteAnimationCycles = 5;

// Download temporary file creation --------------------------------------------

class DefaultDownloadDirectory {
 public:
  const FilePath& path() const { return path_; }
 private:
  DefaultDownloadDirectory() {
    if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &path_)) {
      NOTREACHED();
    }
    if (DownloadPathIsDangerous(path_)) {
      if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS_SAFE, &path_)) {
        NOTREACHED();
      }
    }
  }
  friend struct base::DefaultLazyInstanceTraits<DefaultDownloadDirectory>;
  FilePath path_;
};

static base::LazyInstance<DefaultDownloadDirectory>
    g_default_download_directory = LAZY_INSTANCE_INITIALIZER;

const FilePath& GetDefaultDownloadDirectory() {
  return g_default_download_directory.Get().path();
}

bool DownloadPathIsDangerous(const FilePath& download_path) {
  FilePath desktop_dir;
  if (!PathService::Get(chrome::DIR_USER_DESKTOP, &desktop_dir)) {
    NOTREACHED();
    return false;
  }
  return (download_path == desktop_dir);
}

void GenerateFileNameFromRequest(const DownloadItem& download_item,
                                 FilePath* generated_name) {
  std::string default_file_name(
      l10n_util::GetStringUTF8(IDS_DEFAULT_DOWNLOAD_FILENAME));

  *generated_name = net::GenerateFileName(download_item.GetURL(),
                                          download_item.GetContentDisposition(),
                                          download_item.GetReferrerCharset(),
                                          download_item.GetSuggestedFilename(),
                                          download_item.GetMimeType(),
                                          default_file_name);
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
  int mirrored_x = containing_view->GetMirroredXForRect(background_bounds);
  background_bounds.set_x(mirrored_x);
  mirrored_x = containing_view->GetMirroredXForRect(foreground_bounds);
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
    canvas->GetSkCanvas()->drawPath(path, foreground_paint);
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
  complete_bounds.set_x(containing_view->GetMirroredXForRect(complete_bounds));
#endif

  // Start at full opacity, then loop back and forth five times before ending
  // at zero opacity.
  double opacity = sin(animation_progress * PI * kCompleteAnimationCycles +
                   PI/2) / 2 + 0.5;

  canvas->SaveLayerAlpha(static_cast<int>(255.0 * opacity), complete_bounds);
  canvas->GetSkCanvas()->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);
  canvas->DrawBitmapInt(*complete, complete_bounds.x(), complete_bounds.y());
  canvas->Restore();
}

void PaintDownloadInterrupted(gfx::Canvas* canvas,
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
  complete_bounds.set_x(containing_view->GetMirroredXForRect(complete_bounds));
#endif

  // Start at zero opacity, then loop back and forth five times before ending
  // at full opacity.
  double opacity = sin(
      (1.0 - animation_progress) * PI * kCompleteAnimationCycles + PI/2) / 2 +
          0.5;

  canvas->SaveLayerAlpha(static_cast<int>(255.0 * opacity), complete_bounds);
  canvas->GetSkCanvas()->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);
  canvas->DrawBitmapInt(*complete, complete_bounds.x(), complete_bounds.y());
  canvas->Restore();
}

// Load a language dependent height so that the dangerous download confirmation
// message doesn't overlap with the download link label.
int GetBigProgressIconSize() {
  static int big_progress_icon_size = 0;
  if (big_progress_icon_size == 0) {
    string16 locale_size_str =
        l10n_util::GetStringUTF16(IDS_DOWNLOAD_BIG_PROGRESS_SIZE);
    bool rc = base::StringToInt(locale_size_str, &big_progress_icon_size);
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
                  gfx::Image* icon,
                  gfx::NativeView view) {
  DCHECK(download);

  // Set up our OLE machinery
  ui::OSExchangeData data;

  if (icon) {
    drag_utils::CreateDragImageForFile(
        download->GetFileNameToReportUser(), *icon, &data);
  }

  const FilePath full_path = download->GetFullPath();
  data.SetFilename(full_path);

  std::string mime_type = download->GetMimeType();
  if (mime_type.empty())
    net::GetMimeTypeFromFile(full_path, &mime_type);

  // Add URL so that we can load supported files when dragged to TabContents.
  if (net::IsSupportedMimeType(mime_type)) {
    data.SetURL(net::FilePathToFileURL(full_path),
                download->GetFileNameToReportUser().LossyDisplayName());
  }

#if defined(USE_AURA)
  // TODO(beng):
  NOTIMPLEMENTED();
#elif defined(OS_WIN)
  scoped_refptr<ui::DragSource> drag_source(new ui::DragSource);

  // Run the drag and drop loop
  DWORD effects;
  DoDragDrop(ui::OSExchangeDataProviderWin::GetIDataObject(data),
             drag_source.get(), DROPEFFECT_COPY | DROPEFFECT_LINK, &effects);
#elif defined(TOOLKIT_USES_GTK)
  GtkWidget* root = gtk_widget_get_toplevel(view);
  if (!root)
    return;

  views::NativeWidgetGtk* widget = static_cast<views::NativeWidgetGtk*>(
      views::Widget::GetWidgetForNativeView(root)->native_widget());
  if (!widget)
    return;

  widget->DoDrag(data,
                 ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK);
#endif  // OS_WIN
}
#elif defined(USE_X11)
void DragDownload(const DownloadItem* download,
                  gfx::Image* icon,
                  gfx::NativeView view) {
  DownloadItemDrag::BeginDrag(download, icon);
}
#endif  // USE_X11

DictionaryValue* CreateDownloadItemValue(DownloadItem* download, int id) {
  DictionaryValue* file_value = new DictionaryValue();

  file_value->SetInteger("started",
      static_cast<int>(download->GetStartTime().ToTimeT()));
  file_value->SetString("since_string",
      TimeFormat::RelativeDate(download->GetStartTime(), NULL));
  file_value->SetString("date_string",
      base::TimeFormatShortDate(download->GetStartTime()));
  file_value->SetInteger("id", id);

  FilePath download_path(download->GetTargetFilePath());
  file_value->Set("file_path", base::CreateFilePathValue(download_path));
  file_value->SetString("file_url",
                        net::FilePathToFileURL(download_path).spec());

  // Keep file names as LTR.
  string16 file_name = download->GetFileNameToReportUser().LossyDisplayName();
  file_name = base::i18n::GetDisplayStringInLTRDirectionality(file_name);
  file_value->SetString("file_name", file_name);
  file_value->SetString("url", download->GetURL().spec());
  file_value->SetBoolean("otr", download->IsOtr());
  file_value->SetInteger("total", static_cast<int>(download->GetTotalBytes()));
  file_value->SetBoolean("file_externally_removed",
                         download->GetFileExternallyRemoved());

  if (download->IsInProgress()) {
    if (download->GetSafetyState() == DownloadItem::DANGEROUS) {
      file_value->SetString("state", "DANGEROUS");
      DCHECK(download->GetDangerType() == DownloadStateInfo::DANGEROUS_FILE ||
             download->GetDangerType() == DownloadStateInfo::DANGEROUS_URL);
      const char* danger_type_value =
          download->GetDangerType() == DownloadStateInfo::DANGEROUS_FILE ?
          "DANGEROUS_FILE" : "DANGEROUS_URL";
      file_value->SetString("danger_type", danger_type_value);
    } else if (download->IsPaused()) {
      file_value->SetString("state", "PAUSED");
    } else {
      file_value->SetString("state", "IN_PROGRESS");
    }

    file_value->SetString("progress_status_text",
        GetProgressStatusText(download));

    file_value->SetInteger("percent",
        static_cast<int>(download->PercentComplete()));
    file_value->SetInteger("received",
        static_cast<int>(download->GetReceivedBytes()));
  } else if (download->IsInterrupted()) {
    file_value->SetString("state", "INTERRUPTED");

    file_value->SetString("progress_status_text",
        GetProgressStatusText(download));

    file_value->SetInteger("percent",
        static_cast<int>(download->PercentComplete()));
    file_value->SetInteger("received",
        static_cast<int>(download->GetReceivedBytes()));
  } else if (download->IsCancelled()) {
    file_value->SetString("state", "CANCELLED");
  } else if (download->IsComplete()) {
    if (download->GetSafetyState() == DownloadItem::DANGEROUS)
      file_value->SetString("state", "DANGEROUS");
    else
      file_value->SetString("state", "COMPLETE");
  } else if (download->GetState() == DownloadItem::REMOVING) {
    file_value->SetString("state", "REMOVING");
  } else {
    NOTREACHED() << "state undefined";
  }

  return file_value;
}

string16 GetProgressStatusText(DownloadItem* download) {
  int64 total = download->GetTotalBytes();
  int64 size = download->GetReceivedBytes();
  string16 received_size = ui::FormatBytes(size);
  string16 amount = received_size;

  // Adjust both strings for the locale direction since we don't yet know which
  // string we'll end up using for constructing the final progress string.
  base::i18n::AdjustStringForLocaleDirection(&amount);

  if (total) {
    string16 total_text = ui::FormatBytes(total);
    base::i18n::AdjustStringForLocaleDirection(&total_text);

    base::i18n::AdjustStringForLocaleDirection(&received_size);
    amount = l10n_util::GetStringFUTF16(IDS_DOWNLOAD_TAB_PROGRESS_SIZE,
                                        received_size,
                                        total_text);
  } else {
    amount.assign(received_size);
  }
  int64 current_speed = download->CurrentSpeed();
  string16 speed_text = ui::FormatSpeed(current_speed);
  base::i18n::AdjustStringForLocaleDirection(&speed_text);

  base::TimeDelta remaining;
  string16 time_remaining;
  if (download->IsPaused())
    time_remaining = l10n_util::GetStringUTF16(IDS_DOWNLOAD_PROGRESS_PAUSED);
  else if (download->TimeRemaining(&remaining))
    time_remaining = TimeFormat::TimeRemaining(remaining);

  if (time_remaining.empty()) {
    base::i18n::AdjustStringForLocaleDirection(&amount);
    return l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_TAB_PROGRESS_STATUS_TIME_UNKNOWN, speed_text, amount);
  }
  return l10n_util::GetStringFUTF16(IDS_DOWNLOAD_TAB_PROGRESS_STATUS,
                                    speed_text, amount, time_remaining);
}

#if !defined(OS_MACOSX)
void UpdateAppIconDownloadProgress(int download_count,
                                   bool progress_known,
                                   float progress) {
#if defined(USE_AURA)
  // TODO(davemoore) Implement once UX for download is decided <104742>
#elif defined(OS_WIN)
  // Taskbar progress bar is only supported on Win7.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::win::ScopedComPtr<ITaskbarList3> taskbar;
  HRESULT result = taskbar.CreateInstance(CLSID_TaskbarList, NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(result)) {
    VLOG(1) << "Failed creating a TaskbarList object: " << result;
    return;
  }

  result = taskbar->HrInit();
  if (FAILED(result)) {
    LOG(ERROR) << "Failed initializing an ITaskbarList3 interface.";
    return;
  }

  // Iterate through all the browser windows, and draw the progress bar.
  for (BrowserList::const_iterator browser_iterator = BrowserList::begin();
      browser_iterator != BrowserList::end(); browser_iterator++) {
    Browser* browser = *browser_iterator;
    BrowserWindow* window = browser->window();
    if (!window)
      continue;
    HWND frame = window->GetNativeHandle();
    if (download_count == 0 || progress == 1.0f)
      taskbar->SetProgressState(frame, TBPF_NOPROGRESS);
    else if (!progress_known)
      taskbar->SetProgressState(frame, TBPF_INDETERMINATE);
    else
      taskbar->SetProgressValue(frame, static_cast<int>(progress * 100), 100);
  }
#elif defined(TOOLKIT_GTK)
  unity::SetDownloadCount(download_count);
  unity::SetProgressFraction(progress);
#endif
}
#endif

int GetUniquePathNumberWithCrDownload(const FilePath& path) {
  return DownloadFile::GetUniquePathNumberWithSuffix(
      path, FILE_PATH_LITERAL(".crdownload"));
}

FilePath GetCrDownloadPath(const FilePath& suggested_path) {
  return DownloadFile::AppendSuffixToPath(
      suggested_path, FILE_PATH_LITERAL(".crdownload"));
}

}  // namespace download_util
