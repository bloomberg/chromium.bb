// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download utility implementation

#include "chrome/browser/download/download_util.h"

#include <cmath>
#include <string>

#include "base/file_util.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/download/download_extensions.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/text/bytes_formatting.h"
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
#include "ui/base/dragdrop/drag_source_win.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#endif

namespace download_util {

using content::DownloadItem;

// Download temporary file creation --------------------------------------------

class DefaultDownloadDirectory {
 public:
  const base::FilePath& path() const { return path_; }
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
  base::FilePath path_;
};

static base::LazyInstance<DefaultDownloadDirectory>
    g_default_download_directory = LAZY_INSTANCE_INITIALIZER;

const base::FilePath& GetDefaultDownloadDirectory() {
  return g_default_download_directory.Get().path();
}

// Consider downloads 'dangerous' if they go to the home directory on Linux and
// to the desktop on any platform.
bool DownloadPathIsDangerous(const base::FilePath& download_path) {
#if defined(OS_LINUX)
  base::FilePath home_dir = file_util::GetHomeDir();
  if (download_path == home_dir) {
    return true;
  }
#endif

#if defined(OS_ANDROID)
  // Android does not have a desktop dir.
  return false;
#else
  base::FilePath desktop_dir;
  if (!PathService::Get(base::DIR_USER_DESKTOP, &desktop_dir)) {
    NOTREACHED();
    return false;
  }
  return (download_path == desktop_dir);
#endif
}

#if defined(TOOLKIT_VIEWS)
// Download dragging
void DragDownload(const DownloadItem* download,
                  gfx::Image* icon,
                  gfx::NativeView view) {
  DCHECK(download);
  DCHECK_EQ(DownloadItem::COMPLETE, download->GetState());

  // Set up our OLE machinery
  ui::OSExchangeData data;

  if (icon) {
    drag_utils::CreateDragImageForFile(
        download->GetFileNameToReportUser(), icon->ToImageSkia(), &data);
  }

  const base::FilePath full_path = download->GetTargetFilePath();
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
  scoped_refptr<ui::DragSourceWin> drag_source(new ui::DragSourceWin);
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
    time_remaining = ui::TimeFormat::TimeRemaining(remaining);

  if (time_remaining.empty()) {
    base::i18n::AdjustStringForLocaleDirection(&amount);
    return l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_TAB_PROGRESS_STATUS_TIME_UNKNOWN, speed_text, amount);
  }
  return l10n_util::GetStringFUTF16(IDS_DOWNLOAD_TAB_PROGRESS_STATUS,
                                    speed_text, amount, time_remaining);
}

// Finch trial -----------------------------------------------------------------

const char kFinchTrialName[] = "MalwareDownloadWarning";
const char kCondition1Control[] = "Condition1Control";
const char kCondition2Control[] = "Condition2Control";
const char kCondition3Malicious[] = "Condition3Malicious";
const char kCondition4Unsafe[] = "Condition4Unsafe";
const char kCondition5Dangerous[] = "Condition5Dangerous";
const char kCondition6Harmful[] = "Condition6Harmful";
const char kCondition7DiscardSecond[] = "Condition7DiscardSecond";
const char kCondition8DiscardFirst[] = "Condition8DiscardFirst";
const char kCondition9SafeDiscard[] = "Condition9SafeDiscard";
const char kCondition10SafeDontRun[] = "Condition10SafeDontRun";

base::string16 AssembleMalwareFinchString(
    const std::string& trial_condition, const base::string16& elided_filename) {
  // Sanity check to make sure we have a filename.
  base::string16 filename;
  if (elided_filename.empty()) {
    filename = ASCIIToUTF16("This file");
  } else {
    filename = ReplaceStringPlaceholders(ASCIIToUTF16("File '$1'"),
                                         elided_filename,
                                         NULL);
  }

  // Set the message text according to the condition.
  if (trial_condition == kCondition1Control) {
    return ASCIIToUTF16("This file appears malicious.");
  }
  base::string16 message_text;
  if (trial_condition == kCondition2Control) {
    message_text = ASCIIToUTF16("$1 appears malicious.");
  } else if (trial_condition == kCondition3Malicious) {
    message_text = ASCIIToUTF16("$1 is malicious.");
  } else if (trial_condition == kCondition4Unsafe) {
    message_text = ASCIIToUTF16("$1 is unsafe.");
  } else if (trial_condition == kCondition5Dangerous) {
    message_text = ASCIIToUTF16("$1 is dangerous.");
  } else if (trial_condition == kCondition6Harmful) {
    message_text = ASCIIToUTF16("$1 is harmful.");
  } else if (trial_condition == kCondition7DiscardSecond) {
    message_text = ASCIIToUTF16(
        "$1 is malicious. Discard this file to stay safe.");
  } else if (trial_condition == kCondition8DiscardFirst) {
    message_text = ASCIIToUTF16(
        "Discard this file to stay safe. $1 is malicious.");
  } else if (trial_condition == kCondition9SafeDiscard) {
    message_text = ASCIIToUTF16("$1 is malicious. To stay safe, discard it.");
  } else if (trial_condition == kCondition10SafeDontRun) {
    message_text = ASCIIToUTF16("$1 is malicious. To stay safe, don't run it.");
  } else {
    // We use the second control as a default for other conditions that don't
    // change the warning string.
    message_text = ASCIIToUTF16("$1 appears malicious.");
  }

  return ReplaceStringPlaceholders(message_text, filename, NULL);
}

}  // namespace download_util
