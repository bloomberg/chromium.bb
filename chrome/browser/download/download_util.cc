// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download utility implementation

#include "chrome/browser/download/download_util.h"

#if defined(OS_WIN)
#include <shobjidl.h>
#endif
#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/file_util.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/history/download_create_info.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/time_format.h"
#include "gfx/canvas_skia.h"
#include "gfx/rect.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkShader.h"

#if defined(TOOLKIT_VIEWS)
#include "app/os_exchange_data.h"
#include "views/drag_utils.h"
#endif

#if defined(TOOLKIT_USES_GTK)
#if defined(TOOLKIT_VIEWS)
#include "app/drag_drop_types.h"
#include "views/widget/widget_gtk.h"
#elif defined(TOOLKIT_GTK)
#include "chrome/browser/gtk/custom_drag.h"
#endif  // defined(TOOLKIT_GTK)
#endif  // defined(TOOLKIT_USES_GTK)

#if defined(OS_WIN)
#include "app/os_exchange_data_provider_win.h"
#include "app/win_util.h"
#include "base/base_drag_source.h"
#include "base/registry.h"
#include "base/scoped_comptr_win.h"
#include "base/win_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/views/frame/browser_view.h"
#endif

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
  friend struct DefaultSingletonTraits<DefaultDownloadDirectory>;
  FilePath path_;
};

const FilePath& GetDefaultDownloadDirectory() {
  return Singleton<DefaultDownloadDirectory>::get()->path();
}

bool CreateTemporaryFileForDownload(FilePath* temp_file) {
  if (file_util::CreateTemporaryFileInDir(GetDefaultDownloadDirectory(),
                                          temp_file))
    return true;
  return file_util::CreateTemporaryFile(temp_file);
}

bool DownloadPathIsDangerous(const FilePath& download_path) {
  FilePath desktop_dir;
  if (!PathService::Get(chrome::DIR_USER_DESKTOP, &desktop_dir)) {
    NOTREACHED();
    return false;
  }
  return (download_path == desktop_dir);
}

void GenerateExtension(const FilePath& file_name,
                       const std::string& mime_type,
                       FilePath::StringType* generated_extension) {
  // We're worried about three things here:
  //
  // 1) Security.  Many sites let users upload content, such as buddy icons, to
  //    their web sites.  We want to mitigate the case where an attacker
  //    supplies a malicious executable with an executable file extension but an
  //    honest site serves the content with a benign content type, such as
  //    image/jpeg.
  //
  // 2) Usability.  If the site fails to provide a file extension, we want to
  //    guess a reasonable file extension based on the content type.
  //
  // 3) Shell integration.  Some file extensions automatically integrate with
  //    the shell.  We block these extensions to prevent a malicious web site
  //    from integrating with the user's shell.

  static const FilePath::CharType default_extension[] =
      FILE_PATH_LITERAL("download");

  // See if our file name already contains an extension.
  FilePath::StringType extension = file_name.Extension();
  if (!extension.empty())
    extension.erase(extension.begin());  // Erase preceding '.'.

#if defined(OS_WIN)
  // Rename shell-integrated extensions.
  if (win_util::IsShellIntegratedExtension(extension))
    extension.assign(default_extension);
#endif

  if (IsExecutableExtension(extension) && !IsExecutableMimeType(mime_type)) {
    // We want to be careful about executable extensions.  The worry here is
    // that a trusted web site could be tricked into dropping an executable file
    // on the user's filesystem.
    if (!net::GetPreferredExtensionForMimeType(mime_type, &extension)) {
      // We couldn't find a good extension for this content type.  Use a dummy
      // extension instead.
      extension.assign(default_extension);
    }
  }

  if (extension.empty())
    net::GetPreferredExtensionForMimeType(mime_type, &extension);

  generated_extension->swap(extension);
}

void GenerateFileNameFromInfo(DownloadCreateInfo* info,
                              FilePath* generated_name) {
  GenerateFileName(GURL(info->url),
                   info->content_disposition,
                   info->referrer_charset,
                   info->mime_type,
                   generated_name);
}

void GenerateFileName(const GURL& url,
                      const std::string& content_disposition,
                      const std::string& referrer_charset,
                      const std::string& mime_type,
                      FilePath* generated_name) {
  std::wstring default_name =
      l10n_util::GetString(IDS_DEFAULT_DOWNLOAD_FILENAME);
#if defined(OS_WIN)
  FilePath default_file_path(default_name);
#elif defined(OS_POSIX)
  FilePath default_file_path(base::SysWideToNativeMB(default_name));
#endif

  *generated_name = net::GetSuggestedFilename(GURL(url),
                                              content_disposition,
                                              referrer_charset,
                                              default_file_path);

  DCHECK(!generated_name->empty());

  GenerateSafeFileName(mime_type, generated_name);
}

void GenerateSafeFileName(const std::string& mime_type, FilePath* file_name) {
  // Make sure we get the right file extension
  FilePath::StringType extension;
  GenerateExtension(*file_name, mime_type, &extension);
  *file_name = file_name->ReplaceExtension(extension);

#if defined(OS_WIN)
  // Prepend "_" to the file name if it's a reserved name
  FilePath::StringType leaf_name = file_name->BaseName().value();
  DCHECK(!leaf_name.empty());
  if (win_util::IsReservedName(leaf_name)) {
    leaf_name = FilePath::StringType(FILE_PATH_LITERAL("_")) + leaf_name;
    *file_name = file_name->DirName();
    if (file_name->value() == FilePath::kCurrentDirectory) {
      *file_name = FilePath(leaf_name);
    } else {
      *file_name = file_name->Append(leaf_name);
    }
  }
#endif
}

void OpenChromeExtension(Profile* profile,
                         DownloadManager* download_manager,
                         const DownloadItem& download_item) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(download_item.is_extension_install());

  // We don't support extensions in OTR mode.
  ExtensionsService* service = profile->GetExtensionsService();
  if (service) {
    NotificationService* nservice = NotificationService::current();
    GURL nonconst_download_url = download_item.url();
    nservice->Notify(NotificationType::EXTENSION_READY_FOR_INSTALL,
                     Source<DownloadManager>(download_manager),
                     Details<GURL>(&nonconst_download_url));

    scoped_refptr<CrxInstaller> installer(
        new CrxInstaller(service->install_directory(),
                         service,
                         new ExtensionInstallUI(profile)));
    installer->set_delete_source(true);

    if (UserScript::HasUserScriptFileExtension(download_item.url())) {
      installer->InstallUserScript(download_item.full_path(),
                                   download_item.url());
    } else {
      bool is_gallery_download = ExtensionsService::IsDownloadFromGallery(
          download_item.url(), download_item.referrer_url());
      installer->set_original_mime_type(download_item.original_mime_type());
      installer->set_apps_require_extension_mime_type(true);
      installer->set_allow_privilege_increase(true);
      installer->set_original_url(download_item.url());
      installer->set_limit_web_extent_to_download_host(!is_gallery_download);
      installer->InstallCrx(download_item.full_path());
      installer->set_allow_silent_install(is_gallery_download);
    }
  } else {
    TabContents* contents = NULL;
    // Get last active normal browser of profile.
    Browser* last_active =
        BrowserList::FindBrowserWithType(profile, Browser::TYPE_NORMAL, true);
    if (last_active)
      contents = last_active->GetSelectedTabContents();
    if (contents) {
      contents->AddInfoBar(
          new SimpleAlertInfoBarDelegate(contents,
              l10n_util::GetStringUTF16(
                  IDS_EXTENSION_INCOGNITO_INSTALL_INFOBAR_LABEL),
              ResourceBundle::GetSharedInstance().GetBitmapNamed(
                  IDR_INFOBAR_PLUGIN_INSTALL),
              true));
    }
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
    canvas->AsCanvasSkia()->drawPath(path, foreground_paint);
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

  canvas->SaveLayerAlpha(static_cast<int>(255.0 * opacity), complete_bounds);
  canvas->AsCanvasSkia()->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);
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
                  SkBitmap* icon,
                  gfx::NativeView view) {
  DCHECK(download);

  // Set up our OLE machinery
  OSExchangeData data;

  if (icon) {
    drag_utils::CreateDragImageForFile(download->GetFileName().value(), icon,
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
                     download->GetFileName().ToWStringHack());
  }

#if defined(OS_WIN)
  scoped_refptr<BaseDragSource> drag_source(new BaseDragSource);

  // Run the drag and drop loop
  DWORD effects;
  DoDragDrop(OSExchangeDataProviderWin::GetIDataObject(data), drag_source.get(),
             DROPEFFECT_COPY | DROPEFFECT_LINK, &effects);
#elif defined(TOOLKIT_USES_GTK)
  GtkWidget* root = gtk_widget_get_toplevel(view);
  if (!root)
    return;
  views::WidgetGtk* widget = views::WidgetGtk::GetViewForNative(root);
  if (!widget)
    return;

  widget->DoDrag(data, DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_LINK);
#endif  // OS_WIN
}
#elif defined(USE_X11)
void DragDownload(const DownloadItem* download,
                  SkBitmap* icon,
                  gfx::NativeView view) {
  DownloadItemDrag::BeginDrag(download, icon);
}
#endif  // USE_X11

DictionaryValue* CreateDownloadItemValue(DownloadItem* download, int id) {
  DictionaryValue* file_value = new DictionaryValue();

  file_value->SetInteger("started",
      static_cast<int>(download->start_time().ToTimeT()));
  file_value->SetString("since_string",
      TimeFormat::RelativeDate(download->start_time(), NULL));
  file_value->SetString("date_string",
      WideToUTF16Hack(base::TimeFormatShortDate(download->start_time())));
  file_value->SetInteger("id", id);
  file_value->SetString("file_path",
      WideToUTF16Hack(download->full_path().ToWStringHack()));
  // Keep file names as LTR.
  string16 file_name = WideToUTF16Hack(
      download->GetFileName().ToWStringHack());
  file_name = base::i18n::GetDisplayStringInLTRDirectionality(file_name);
  file_value->SetString("file_name", file_name);
  file_value->SetString("url", download->url().spec());
  file_value->SetBoolean("otr", download->is_otr());

  if (download->state() == DownloadItem::IN_PROGRESS) {
    if (download->safety_state() == DownloadItem::DANGEROUS) {
      file_value->SetString("state", "DANGEROUS");
    } else if (download->is_paused()) {
      file_value->SetString("state", "PAUSED");
    } else {
      file_value->SetString("state", "IN_PROGRESS");
    }

    file_value->SetString("progress_status_text",
       WideToUTF16Hack(GetProgressStatusText(download)));

    file_value->SetInteger("percent",
        static_cast<int>(download->PercentComplete()));
    file_value->SetInteger("received",
        static_cast<int>(download->received_bytes()));
  } else if (download->state() == DownloadItem::CANCELLED) {
    file_value->SetString("state", "CANCELLED");
  } else if (download->state() == DownloadItem::COMPLETE) {
    if (download->safety_state() == DownloadItem::DANGEROUS) {
      file_value->SetString("state", "DANGEROUS");
    } else {
      file_value->SetString("state", "COMPLETE");
    }
  }

  file_value->SetInteger("total",
      static_cast<int>(download->total_bytes()));

  return file_value;
}

std::wstring GetProgressStatusText(DownloadItem* download) {
  int64 total = download->total_bytes();
  int64 size = download->received_bytes();
  DataUnits amount_units = GetByteDisplayUnits(size);
  std::wstring received_size = UTF16ToWideHack(FormatBytes(size, amount_units,
                                                           true));
  std::wstring amount = received_size;

  // Adjust both strings for the locale direction since we don't yet know which
  // string we'll end up using for constructing the final progress string.
  std::wstring amount_localized;
  if (base::i18n::AdjustStringForLocaleDirection(amount, &amount_localized)) {
    amount.assign(amount_localized);
    received_size.assign(amount_localized);
  }

  if (total) {
    amount_units = GetByteDisplayUnits(total);
    std::wstring total_text =
        UTF16ToWideHack(FormatBytes(total, amount_units, true));
    std::wstring total_text_localized;
    if (base::i18n::AdjustStringForLocaleDirection(total_text,
                                                   &total_text_localized))
      total_text.assign(total_text_localized);

    amount = l10n_util::GetStringF(IDS_DOWNLOAD_TAB_PROGRESS_SIZE,
                                   received_size,
                                   total_text);
  } else {
    amount.assign(received_size);
  }
  amount_units = GetByteDisplayUnits(download->CurrentSpeed());
  std::wstring speed_text =
      UTF16ToWideHack(FormatSpeed(download->CurrentSpeed(), amount_units,
                                  true));
  std::wstring speed_text_localized;
  if (base::i18n::AdjustStringForLocaleDirection(speed_text,
                                                 &speed_text_localized))
    speed_text.assign(speed_text_localized);

  base::TimeDelta remaining;
  string16 time_remaining;
  if (download->is_paused())
    time_remaining = l10n_util::GetStringUTF16(IDS_DOWNLOAD_PROGRESS_PAUSED);
  else if (download->TimeRemaining(&remaining))
    time_remaining = TimeFormat::TimeRemaining(remaining);

  if (time_remaining.empty()) {
    return l10n_util::GetStringF(IDS_DOWNLOAD_TAB_PROGRESS_STATUS_TIME_UNKNOWN,
                                 speed_text, amount);
  }
  return l10n_util::GetStringF(IDS_DOWNLOAD_TAB_PROGRESS_STATUS, speed_text,
                               amount, UTF16ToWideHack(time_remaining));
}

#if !defined(OS_MACOSX)
void UpdateAppIconDownloadProgress(int download_count,
                                   bool progress_known,
                                   float progress) {
#if defined(OS_WIN)
  // Taskbar progress bar is only supported on Win7.
  if (win_util::GetWinVersion() < win_util::WINVERSION_WIN7)
    return;

  ScopedComPtr<ITaskbarList3> taskbar;
  HRESULT result = taskbar.CreateInstance(CLSID_TaskbarList, NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(result)) {
    LOG(INFO) << "failed creating a TaskbarList object: " << result;
    return;
  }

  result = taskbar->HrInit();
  if (FAILED(result)) {
    LOG(ERROR) << "failed initializing an ITaskbarList3 interface.";
    return;
  }

  // Iterate through all the browser windows, and draw the progress bar.
  for (BrowserList::const_iterator browser_iterator = BrowserList::begin();
      browser_iterator != BrowserList::end(); browser_iterator++) {
    HWND frame = (*browser_iterator)->window()->GetNativeHandle();
    if (download_count == 0 || progress == 1.0f)
      taskbar->SetProgressState(frame, TBPF_NOPROGRESS);
    else if (!progress_known)
      taskbar->SetProgressState(frame, TBPF_INDETERMINATE);
    else
      taskbar->SetProgressValue(frame, (int)(progress * 100), 100);
  }
#endif
}
#endif

// Appends the passed the number between parenthesis the path before the
// extension.
void AppendNumberToPath(FilePath* path, int number) {
  *path = path->InsertBeforeExtensionASCII(StringPrintf(" (%d)", number));
}

// Attempts to find a number that can be appended to that path to make it
// unique. If |path| does not exist, 0 is returned.  If it fails to find such
// a number, -1 is returned.
int GetUniquePathNumber(const FilePath& path) {
  const int kMaxAttempts = 100;

  if (!file_util::PathExists(path))
    return 0;

  FilePath new_path;
  for (int count = 1; count <= kMaxAttempts; ++count) {
    new_path = FilePath(path);
    AppendNumberToPath(&new_path, count);

    if (!file_util::PathExists(new_path))
      return count;
  }

  return -1;
}

void DownloadUrl(
    const GURL& url,
    const GURL& referrer,
    const std::string& referrer_charset,
    const DownloadSaveInfo& save_info,
    ResourceDispatcherHost* rdh,
    int render_process_host_id,
    int render_view_id,
    URLRequestContextGetter* request_context_getter) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  URLRequestContext* context = request_context_getter->GetURLRequestContext();
  context->set_referrer_charset(referrer_charset);

  rdh->BeginDownload(url,
                     referrer,
                     save_info,
                     true,  // Show "Save as" UI.
                     render_process_host_id,
                     render_view_id,
                     context);
}

void CancelDownloadRequest(ResourceDispatcherHost* rdh,
                           int render_process_id,
                           int request_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  rdh->CancelRequest(render_process_id, request_id, false);
}

int GetUniquePathNumberWithCrDownload(const FilePath& path) {
  const int kMaxAttempts = 100;

  if (!file_util::PathExists(path) &&
      !file_util::PathExists(GetCrDownloadPath(path)))
    return 0;

  FilePath new_path;
  for (int count = 1; count <= kMaxAttempts; ++count) {
    new_path = FilePath(path);
    AppendNumberToPath(&new_path, count);

    if (!file_util::PathExists(new_path) &&
        !file_util::PathExists(GetCrDownloadPath(new_path)))
      return count;
  }

  return -1;
}

FilePath GetCrDownloadPath(const FilePath& suggested_path) {
  FilePath::StringType file_name;
  SStringPrintf(&file_name, PRFilePathLiteral FILE_PATH_LITERAL(".crdownload"),
                suggested_path.value().c_str());
  return FilePath(file_name);
}

}  // namespace download_util
