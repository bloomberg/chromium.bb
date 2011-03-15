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
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/value_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/browser/download/download_extensions.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/history/download_create_info.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/time_format.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
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
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/image.h"
#include "ui/gfx/rect.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/base/dragdrop/os_exchange_data.h"
#include "views/drag_utils.h"
#endif

#if defined(TOOLKIT_USES_GTK)
#if defined(TOOLKIT_VIEWS)
#include "ui/base/dragdrop/drag_drop_types.h"
#include "views/widget/widget_gtk.h"
#elif defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/custom_drag.h"
#endif  // defined(TOOLKIT_GTK)
#endif  // defined(TOOLKIT_USES_GTK)

#if defined(OS_WIN)
#include "base/win/scoped_comptr.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/dragdrop/drag_source.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#endif

namespace download_util {

// How many times to cycle the complete animation. This should be an odd number
// so that the animation ends faded out.
static const int kCompleteAnimationCycles = 5;

// The maximum number of 'uniquified' files we will try to create.
// This is used when the filename we're trying to download is already in use,
// so we create a new unique filename by appending " (nnn)" before the
// extension, where 1 <= nnn <= kMaxUniqueFiles.
// Also used by code that cleans up said files.
static const int kMaxUniqueFiles = 100;

namespace {

#if defined(OS_WIN)
// Returns whether the specified extension is automatically integrated into the
// windows shell.
bool IsShellIntegratedExtension(const string16& extension) {
  string16 extension_lower = StringToLowerASCII(extension);

  static const wchar_t* const integrated_extensions[] = {
    // See <http://msdn.microsoft.com/en-us/library/ms811694.aspx>.
    L"local",
    // Right-clicking on shortcuts can be magical.
    L"lnk",
  };

  for (int i = 0; i < arraysize(integrated_extensions); ++i) {
    if (extension_lower == integrated_extensions[i])
      return true;
  }

  // See <http://www.juniper.net/security/auto/vulnerabilities/vuln2612.html>.
  // That vulnerability report is not exactly on point, but files become magical
  // if their end in a CLSID.  Here we block extensions that look like CLSIDs.
  if (!extension_lower.empty() && extension_lower[0] == L'{' &&
      extension_lower[extension_lower.length() - 1] == L'}')
    return true;

  return false;
}

// Returns whether the specified file name is a reserved name on windows.
// This includes names like "com2.zip" (which correspond to devices) and
// desktop.ini and thumbs.db which have special meaning to the windows shell.
bool IsReservedName(const string16& filename) {
  // This list is taken from the MSDN article "Naming a file"
  // http://msdn2.microsoft.com/en-us/library/aa365247(VS.85).aspx
  // I also added clock$ because GetSaveFileName seems to consider it as a
  // reserved name too.
  static const wchar_t* const known_devices[] = {
    L"con", L"prn", L"aux", L"nul", L"com1", L"com2", L"com3", L"com4", L"com5",
    L"com6", L"com7", L"com8", L"com9", L"lpt1", L"lpt2", L"lpt3", L"lpt4",
    L"lpt5", L"lpt6", L"lpt7", L"lpt8", L"lpt9", L"clock$"
  };
  string16 filename_lower = StringToLowerASCII(filename);

  for (int i = 0; i < arraysize(known_devices); ++i) {
    // Exact match.
    if (filename_lower == known_devices[i])
      return true;
    // Starts with "DEVICE.".
    if (filename_lower.find(string16(known_devices[i]) + L".") == 0)
      return true;
  }

  static const wchar_t* const magic_names[] = {
    // These file names are used by the "Customize folder" feature of the shell.
    L"desktop.ini",
    L"thumbs.db",
  };

  for (int i = 0; i < arraysize(magic_names); ++i) {
    if (filename_lower == magic_names[i])
      return true;
  }

  return false;
}
#endif  // OS_WIN

}  // namespace

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
    g_default_download_directory(base::LINKER_INITIALIZED);

const FilePath& GetDefaultDownloadDirectory() {
  return g_default_download_directory.Get().path();
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
  // We're worried about two things here:
  //
  // 1) Usability.  If the site fails to provide a file extension, we want to
  //    guess a reasonable file extension based on the content type.
  //
  // 2) Shell integration.  Some file extensions automatically integrate with
  //    the shell.  We block these extensions to prevent a malicious web site
  //    from integrating with the user's shell.

  // See if our file name already contains an extension.
  FilePath::StringType extension = file_name.Extension();
  if (!extension.empty())
    extension.erase(extension.begin());  // Erase preceding '.'.

#if defined(OS_WIN)
  static const FilePath::CharType default_extension[] =
      FILE_PATH_LITERAL("download");

  // Rename shell-integrated extensions.
  if (IsShellIntegratedExtension(extension))
    extension.assign(default_extension);
#endif

  if (extension.empty()) {
    // The GetPreferredExtensionForMimeType call will end up going to disk.  Do
    // this on another thread to avoid slowing the IO thread.
    // http://crbug.com/61827
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    net::GetPreferredExtensionForMimeType(mime_type, &extension);
  }

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
  string16 default_file_name(
      l10n_util::GetStringUTF16(IDS_DEFAULT_DOWNLOAD_FILENAME));

  string16 new_name = net::GetSuggestedFilename(GURL(url),
                                                content_disposition,
                                                referrer_charset,
                                                default_file_name);

  // TODO(evan): this code is totally wrong -- we should just generate
  // Unicode filenames and do all this encoding switching at the end.
  // However, I'm just shuffling wrong code around, at least not adding
  // to it.
#if defined(OS_WIN)
  *generated_name = FilePath(new_name);
#else
  *generated_name = FilePath(
      base::SysWideToNativeMB(UTF16ToWide(new_name)));
#endif

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
  if (IsReservedName(leaf_name)) {
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(download_item.is_extension_install());

  ExtensionService* service = profile->GetExtensionService();
  CHECK(service);
  NotificationService* nservice = NotificationService::current();
  GURL nonconst_download_url = download_item.url();
  nservice->Notify(NotificationType::EXTENSION_READY_FOR_INSTALL,
                   Source<DownloadManager>(download_manager),
                   Details<GURL>(&nonconst_download_url));

  scoped_refptr<CrxInstaller> installer(
      new CrxInstaller(service, new ExtensionInstallUI(profile)));
  installer->set_delete_source(true);

  if (UserScript::IsURLUserScript(download_item.url(),
                                  download_item.mime_type())) {
    installer->InstallUserScript(download_item.full_path(),
                                 download_item.url());
    return;
  }

  bool is_gallery_download = service->IsDownloadFromGallery(
      download_item.url(), download_item.referrer_url());
  installer->set_original_mime_type(download_item.original_mime_type());
  installer->set_apps_require_extension_mime_type(true);
  installer->set_original_url(download_item.url());
  installer->set_is_gallery_install(is_gallery_download);
  installer->InstallCrx(download_item.full_path());
  installer->set_allow_silent_install(is_gallery_download);
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
  complete_bounds.set_x(containing_view->GetMirroredXForRect(complete_bounds));
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
                  gfx::Image* icon,
                  gfx::NativeView view) {
  DCHECK(download);

  // Set up our OLE machinery
  ui::OSExchangeData data;

  if (icon) {
    drag_utils::CreateDragImageForFile(
        download->GetFileNameToReportUser(), *icon, &data);
  }

  const FilePath full_path = download->full_path();
  data.SetFilename(full_path);

  std::string mime_type = download->mime_type();
  if (mime_type.empty())
    net::GetMimeTypeFromFile(full_path, &mime_type);

  // Add URL so that we can load supported files when dragged to TabContents.
  if (net::IsSupportedMimeType(mime_type)) {
    data.SetURL(net::FilePathToFileURL(full_path),
                download->GetFileNameToReportUser().LossyDisplayName());
  }

#if defined(OS_WIN)
  scoped_refptr<ui::DragSource> drag_source(new ui::DragSource);

  // Run the drag and drop loop
  DWORD effects;
  DoDragDrop(ui::OSExchangeDataProviderWin::GetIDataObject(data),
             drag_source.get(), DROPEFFECT_COPY | DROPEFFECT_LINK, &effects);
#elif defined(TOOLKIT_USES_GTK)
  GtkWidget* root = gtk_widget_get_toplevel(view);
  if (!root)
    return;

  views::WidgetGtk* widget = static_cast<views::WidgetGtk*>(
      views::NativeWidget::GetNativeWidgetForNativeView(root));
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
      static_cast<int>(download->start_time().ToTimeT()));
  file_value->SetString("since_string",
      TimeFormat::RelativeDate(download->start_time(), NULL));
  file_value->SetString("date_string",
      base::TimeFormatShortDate(download->start_time()));
  file_value->SetInteger("id", id);
  file_value->Set("file_path",
                  base::CreateFilePathValue(download->GetTargetFilePath()));
  // Keep file names as LTR.
  string16 file_name = download->GetFileNameToReportUser().LossyDisplayName();
  file_name = base::i18n::GetDisplayStringInLTRDirectionality(file_name);
  file_value->SetString("file_name", file_name);
  file_value->SetString("url", download->url().spec());
  file_value->SetBoolean("otr", download->is_otr());

  if (download->state() == DownloadItem::IN_PROGRESS) {
    if (download->safety_state() == DownloadItem::DANGEROUS) {
      file_value->SetString("state", "DANGEROUS");
      DCHECK(download->danger_type() == DownloadItem::DANGEROUS_FILE ||
             download->danger_type() == DownloadItem::DANGEROUS_URL);
      const char* danger_type_value =
          download->danger_type() == DownloadItem::DANGEROUS_FILE ?
          "DANGEROUS_FILE" : "DANGEROUS_URL";
      file_value->SetString("danger_type", danger_type_value);
    } else if (download->is_paused()) {
      file_value->SetString("state", "PAUSED");
    } else {
      file_value->SetString("state", "IN_PROGRESS");
    }

    file_value->SetString("progress_status_text",
       GetProgressStatusText(download));

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

string16 GetProgressStatusText(DownloadItem* download) {
  int64 total = download->total_bytes();
  int64 size = download->received_bytes();
  DataUnits amount_units = GetByteDisplayUnits(size);
  string16 received_size = FormatBytes(size, amount_units, true);
  string16 amount = received_size;

  // Adjust both strings for the locale direction since we don't yet know which
  // string we'll end up using for constructing the final progress string.
  base::i18n::AdjustStringForLocaleDirection(&amount);

  if (total) {
    amount_units = GetByteDisplayUnits(total);
    string16 total_text = FormatBytes(total, amount_units, true);
    base::i18n::AdjustStringForLocaleDirection(&total_text);

    base::i18n::AdjustStringForLocaleDirection(&received_size);
    amount = l10n_util::GetStringFUTF16(IDS_DOWNLOAD_TAB_PROGRESS_SIZE,
                                        received_size,
                                        total_text);
  } else {
    amount.assign(received_size);
  }
  int64 current_speed = download->CurrentSpeed();
  amount_units = GetByteDisplayUnits(current_speed);
  string16 speed_text = FormatSpeed(current_speed, amount_units, true);
  base::i18n::AdjustStringForLocaleDirection(&speed_text);

  base::TimeDelta remaining;
  string16 time_remaining;
  if (download->is_paused())
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
#if defined(OS_WIN)
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
  if (!file_util::PathExists(path))
    return 0;

  FilePath new_path;
  for (int count = 1; count <= kMaxUniqueFiles; ++count) {
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::URLRequestContext* context =
      request_context_getter->GetURLRequestContext();
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  rdh->CancelRequest(render_process_id, request_id, false);
}

void NotifyDownloadInitiated(int render_process_id, int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderViewHost* rvh = RenderViewHost::FromID(render_process_id,
                                               render_view_id);
  if (!rvh)
    return;

  NotificationService::current()->Notify(NotificationType::DOWNLOAD_INITIATED,
                                         Source<RenderViewHost>(rvh),
                                         NotificationService::NoDetails());
}

int GetUniquePathNumberWithCrDownload(const FilePath& path) {
  if (!file_util::PathExists(path) &&
      !file_util::PathExists(GetCrDownloadPath(path)))
    return 0;

  FilePath new_path;
  for (int count = 1; count <= kMaxUniqueFiles; ++count) {
    new_path = FilePath(path);
    AppendNumberToPath(&new_path, count);

    if (!file_util::PathExists(new_path) &&
        !file_util::PathExists(GetCrDownloadPath(new_path)))
      return count;
  }

  return -1;
}

namespace {

// NOTE: If index is 0, deletes files that do not have the " (nnn)" appended.
void DeleteUniqueDownloadFile(const FilePath& path, int index) {
  FilePath new_path(path);
  if (index > 0)
    AppendNumberToPath(&new_path, index);
  file_util::Delete(new_path, false);
}

}  // namespace

void EraseUniqueDownloadFiles(const FilePath& path) {
  FilePath cr_path = GetCrDownloadPath(path);

  for (int index = 0; index <= kMaxUniqueFiles; ++index) {
    DeleteUniqueDownloadFile(path, index);
    DeleteUniqueDownloadFile(cr_path, index);
  }
}

FilePath GetCrDownloadPath(const FilePath& suggested_path) {
  FilePath::StringType file_name;
  base::SStringPrintf(
      &file_name,
      PRFilePathLiteral FILE_PATH_LITERAL(".crdownload"),
      suggested_path.value().c_str());
  return FilePath(file_name);
}

// TODO(erikkay,phajdan.jr): This is apparently not being exercised in tests.
bool IsDangerous(DownloadCreateInfo* info, Profile* profile, bool auto_open) {
  DownloadDangerLevel danger_level = GetFileDangerLevel(
      info->suggested_path.BaseName());
  if (danger_level == Dangerous)
    return !(auto_open && info->has_user_gesture);
  if (danger_level == AllowOnUserGesture && !info->has_user_gesture)
    return true;
  if (info->is_extension_install) {
    // Extensions that are not from the gallery are considered dangerous.
    ExtensionService* service = profile->GetExtensionService();
    if (!service ||
        !service->IsDownloadFromGallery(info->url, info->referrer_url))
      return true;
  }
  return false;
}

}  // namespace download_util
