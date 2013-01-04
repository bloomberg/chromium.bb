// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download utility implementation

#define _USE_MATH_DEFINES  // For VC++ to get M_PI. This has to be first.

#include "chrome/browser/download/download_util.h"

#include <cmath>
#include <string>

#include "base/file_util.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/download/download_extensions.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/time_format.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/url_constants.h"
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
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"
#endif

#if defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/custom_drag.h"
#endif  // defined(TOOLKIT_GTK)

#if defined(OS_WIN) && !defined(USE_AURA)
#include "ui/base/dragdrop/drag_source.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#endif

namespace {

// Get the opacity based on |animation_progress|, with values in [0.0, 1.0].
// Range of return value is [0, 255].
int GetOpacity(double animation_progress) {
  DCHECK(animation_progress >= 0 && animation_progress <= 1);

  // How many times to cycle the complete animation. This should be an odd
  // number so that the animation ends faded out.
  static const int kCompleteAnimationCycles = 5;
  double temp = animation_progress * kCompleteAnimationCycles * M_PI + M_PI_2;
  temp = sin(temp) / 2 + 0.5;
  return static_cast<int>(255.0 * temp);
}

}  // namespace

namespace download_util {

using content::DownloadItem;

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
      // This is only useful on platforms that support
      // DIR_DEFAULT_DOWNLOADS_SAFE.
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

// Consider downloads 'dangerous' if they go to the home directory on Linux and
// to the desktop on any platform.
bool DownloadPathIsDangerous(const FilePath& download_path) {
#if defined(OS_LINUX)
  FilePath home_dir = file_util::GetHomeDir();
  if (download_path == home_dir) {
    return true;
  }
#endif

#if defined(OS_ANDROID)
  // Android does not have a desktop dir.
  return false;
#else
  FilePath desktop_dir;
  if (!PathService::Get(base::DIR_USER_DESKTOP, &desktop_dir)) {
    NOTREACHED();
    return false;
  }
  return (download_path == desktop_dir);
#endif
}

// Download progress painting --------------------------------------------------

// Common images used for download progress animations. We load them once the
// first time we do a progress paint, then reuse them as they are always the
// same.
gfx::ImageSkia* g_foreground_16 = NULL;
gfx::ImageSkia* g_background_16 = NULL;
gfx::ImageSkia* g_foreground_32 = NULL;
gfx::ImageSkia* g_background_32 = NULL;

void PaintCustomDownloadProgress(gfx::Canvas* canvas,
                                 const gfx::ImageSkia& background_image,
                                 const gfx::ImageSkia& foreground_image,
                                 int image_size,
                                 const gfx::Rect& bounds,
                                 int start_angle,
                                 int percent_done) {
  // Draw the background progress image.
  canvas->DrawImageInt(background_image,
                       bounds.x(),
                       bounds.y());

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
  canvas->Save();
  if (sweep_angle < static_cast<float>(kMaxDegrees - 1)) {
    SkRect oval;
    oval.set(SkIntToScalar(bounds.x()),
             SkIntToScalar(bounds.y()),
             SkIntToScalar(bounds.x() + image_size),
             SkIntToScalar(bounds.y() + image_size));
    SkPath path;
    path.arcTo(oval,
               SkFloatToScalar(start_pos),
               SkFloatToScalar(sweep_angle), false);
    path.lineTo(SkIntToScalar(bounds.x() + image_size / 2),
                SkIntToScalar(bounds.y() + image_size / 2));

    // gfx::Canvas::ClipPath does not provide for anti-aliasing.
    canvas->sk_canvas()->clipPath(path, SkRegion::kIntersect_Op, true);
  }

  canvas->DrawImageInt(foreground_image,
                       bounds.x(),
                       bounds.y());
  canvas->Restore();
}


void PaintDownloadProgress(gfx::Canvas* canvas,
#if defined(TOOLKIT_VIEWS)
                           views::View* containing_view,
#endif
                           int origin_x,
                           int origin_y,
                           int start_angle,
                           int percent_done,
                           PaintDownloadProgressSize size) {
  // Load up our common images.
  if (!g_background_16) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    g_foreground_16 = rb.GetImageSkiaNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_16);
    g_background_16 = rb.GetImageSkiaNamed(IDR_DOWNLOAD_PROGRESS_BACKGROUND_16);
    g_foreground_32 = rb.GetImageSkiaNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_32);
    g_background_32 = rb.GetImageSkiaNamed(IDR_DOWNLOAD_PROGRESS_BACKGROUND_32);
    DCHECK_EQ(g_foreground_16->width(), g_background_16->width());
    DCHECK_EQ(g_foreground_16->height(), g_background_16->height());
    DCHECK_EQ(g_foreground_32->width(), g_background_32->width());
    DCHECK_EQ(g_foreground_32->height(), g_background_32->height());
  }

  gfx::ImageSkia* background =
      (size == BIG) ? g_background_32 : g_background_16;
  gfx::ImageSkia* foreground =
      (size == BIG) ? g_foreground_32 : g_foreground_16;

  const int kProgressIconSize = (size == BIG) ? kBigProgressIconSize :
                                                kSmallProgressIconSize;

  // We start by storing the bounds of the images so that it is easy to mirror
  // the bounds if the UI layout is RTL.
  gfx::Rect bounds(origin_x, origin_y,
                   background->width(), background->height());

#if defined(TOOLKIT_VIEWS)
  // Mirror the positions if necessary.
  int mirrored_x = containing_view->GetMirroredXForRect(bounds);
  bounds.set_x(mirrored_x);
#endif

  // Draw the background progress image.
  canvas->DrawImageInt(*background,
                       bounds.x(),
                       bounds.y());

  PaintCustomDownloadProgress(canvas, *background, *foreground,
                              kProgressIconSize, bounds, start_angle,
                              percent_done);
}

void PaintDownloadComplete(gfx::Canvas* canvas,
#if defined(TOOLKIT_VIEWS)
                           views::View* containing_view,
#endif
                           int origin_x,
                           int origin_y,
                           double animation_progress,
                           PaintDownloadProgressSize size) {
  // Load up our common images.
  if (!g_foreground_16) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    g_foreground_16 = rb.GetImageSkiaNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_16);
    g_foreground_32 = rb.GetImageSkiaNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_32);
  }

  gfx::ImageSkia* complete = (size == BIG) ? g_foreground_32 : g_foreground_16;

  gfx::Rect complete_bounds(origin_x, origin_y,
                            complete->width(), complete->height());
#if defined(TOOLKIT_VIEWS)
  // Mirror the positions if necessary.
  complete_bounds.set_x(containing_view->GetMirroredXForRect(complete_bounds));
#endif

  // Start at full opacity, then loop back and forth five times before ending
  // at zero opacity.
  canvas->DrawImageInt(*complete, complete_bounds.x(), complete_bounds.y(),
                       GetOpacity(animation_progress));
}

void PaintDownloadInterrupted(gfx::Canvas* canvas,
#if defined(TOOLKIT_VIEWS)
                              views::View* containing_view,
#endif
                              int origin_x,
                              int origin_y,
                              double animation_progress,
                              PaintDownloadProgressSize size) {
  // Load up our common images.
  if (!g_foreground_16) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    g_foreground_16 = rb.GetImageSkiaNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_16);
    g_foreground_32 = rb.GetImageSkiaNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_32);
  }

  gfx::ImageSkia* complete = (size == BIG) ? g_foreground_32 : g_foreground_16;

  gfx::Rect complete_bounds(origin_x, origin_y,
                            complete->width(), complete->height());
#if defined(TOOLKIT_VIEWS)
  // Mirror the positions if necessary.
  complete_bounds.set_x(containing_view->GetMirroredXForRect(complete_bounds));
#endif

  // Start at zero opacity, then loop back and forth five times before ending
  // at full opacity.
  canvas->DrawImageInt(*complete, complete_bounds.x(), complete_bounds.y(),
                       GetOpacity(1.0 - animation_progress));
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
        download->GetFileNameToReportUser(), icon->ToImageSkia(), &data);
  }

  const FilePath full_path = download->GetFullPath();
  data.SetFilename(full_path);

  std::string mime_type = download->GetMimeType();
  if (mime_type.empty())
    net::GetMimeTypeFromFile(full_path, &mime_type);

  // Add URL so that we can load supported files when dragged to WebContents.
  if (net::IsSupportedMimeType(mime_type)) {
    data.SetURL(net::FilePathToFileURL(full_path),
                download->GetFileNameToReportUser().LossyDisplayName());
  }

#if !defined(TOOLKIT_GTK)
#if defined(USE_AURA)
  aura::RootWindow* root_window = view->GetRootWindow();
  if (!root_window || !aura::client::GetDragDropClient(root_window))
    return;

  gfx::Point location = gfx::Screen::GetScreenFor(view)->GetCursorScreenPoint();
  // TODO(varunjain): Properly determine and send DRAG_EVENT_SOURCE below.
  aura::client::GetDragDropClient(root_window)->StartDragAndDrop(
      data,
      root_window,
      view,
      location,
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK,
      ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
#else  // We are on WIN without AURA
  // We cannot use Widget::RunShellDrag on WIN since the |view| is backed by a
  // WebContentsViewWin, not a NativeWidgetWin.
  scoped_refptr<ui::DragSource> drag_source(new ui::DragSource);
  // Run the drag and drop loop
  DWORD effects;
  DoDragDrop(ui::OSExchangeDataProviderWin::GetIDataObject(data),
             drag_source.get(), DROPEFFECT_COPY | DROPEFFECT_LINK, &effects);
#endif

#else
  GtkWidget* root = gtk_widget_get_toplevel(view);
  if (!root)
    return;

  views::NativeWidgetGtk* widget = static_cast<views::NativeWidgetGtk*>(
      views::Widget::GetWidgetForNativeView(root)->native_widget());
  if (!widget)
    return;

  widget->DoDrag(data,
                 ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK);
#endif  // TOOLKIT_GTK
}
#elif defined(USE_X11)
void DragDownload(const DownloadItem* download,
                  gfx::Image* icon,
                  gfx::NativeView view) {
  DownloadItemDrag::BeginDrag(download, icon);
}
#endif  // USE_X11

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

FilePath GetCrDownloadPath(const FilePath& suggested_path) {
  return FilePath(suggested_path.value() + FILE_PATH_LITERAL(".crdownload"));
}

bool IsSavableURL(const GURL& url) {
  for (int i = 0; content::GetSavableSchemes()[i] != NULL; ++i) {
    if (url.SchemeIs(content::GetSavableSchemes()[i])) {
      return true;
    }
  }
  return false;
}

void RecordShelfClose(int size, int in_progress, bool autoclose) {
  static const int kMaxShelfSize = 16;
  if (autoclose) {
    UMA_HISTOGRAM_ENUMERATION("Download.ShelfSizeOnAutoClose",
                              size,
                              kMaxShelfSize);
    UMA_HISTOGRAM_ENUMERATION("Download.ShelfInProgressSizeOnAutoClose",
                              in_progress,
                              kMaxShelfSize);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Download.ShelfSizeOnUserClose",
                              size,
                              kMaxShelfSize);
    UMA_HISTOGRAM_ENUMERATION("Download.ShelfInProgressSizeOnUserClose",
                              in_progress,
                              kMaxShelfSize);
  }
}

void RecordDownloadCount(ChromeDownloadCountTypes type) {
  UMA_HISTOGRAM_ENUMERATION(
      "Download.CountsChrome", type, CHROME_DOWNLOAD_COUNT_TYPES_LAST_ENTRY);
}

void RecordDownloadSource(ChromeDownloadSource source) {
  UMA_HISTOGRAM_ENUMERATION(
      "Download.SourcesChrome", source, CHROME_DOWNLOAD_SOURCE_LAST_ENTRY);
}

}  // namespace download_util
