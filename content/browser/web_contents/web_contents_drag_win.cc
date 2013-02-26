// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_drag_win.h"

#include <windows.h>

#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/message_loop.h"
#include "base/pickle.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "content/browser/download/drag_download_file.h"
#include "content/browser/download/drag_download_util.h"
#include "content/browser/web_contents/web_drag_dest_win.h"
#include "content/browser/web_contents/web_drag_source_win.h"
#include "content/browser/web_contents/web_drag_utils_win.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_drag_dest_delegate.h"
#include "net/base/net_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_util_win.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/layout.h"
#include "ui/base/win/scoped_ole_initializer.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "webkit/glue/webdropdata.h"

using WebKit::WebDragOperationsMask;
using WebKit::WebDragOperationCopy;
using WebKit::WebDragOperationLink;
using WebKit::WebDragOperationMove;

namespace content {
namespace {

HHOOK msg_hook = NULL;
DWORD drag_out_thread_id = 0;
bool mouse_up_received = false;

LRESULT CALLBACK MsgFilterProc(int code, WPARAM wparam, LPARAM lparam) {
  if (code == base::MessagePumpForUI::kMessageFilterCode &&
      !mouse_up_received) {
    MSG* msg = reinterpret_cast<MSG*>(lparam);
    // We do not care about WM_SYSKEYDOWN and WM_SYSKEYUP because when ALT key
    // is pressed down on drag-and-drop, it means to create a link.
    if (msg->message == WM_MOUSEMOVE || msg->message == WM_LBUTTONUP ||
        msg->message == WM_KEYDOWN || msg->message == WM_KEYUP) {
      // Forward the message from the UI thread to the drag-and-drop thread.
      PostThreadMessage(drag_out_thread_id,
                        msg->message,
                        msg->wParam,
                        msg->lParam);

      // If the left button is up, we do not need to forward the message any
      // more.
      if (msg->message == WM_LBUTTONUP || !(GetKeyState(VK_LBUTTON) & 0x8000))
        mouse_up_received = true;

      return TRUE;
    }
  }
  return CallNextHookEx(msg_hook, code, wparam, lparam);
}

void EnableBackgroundDraggingSupport(DWORD thread_id) {
  // Install a hook procedure to monitor the messages so that we can forward
  // the appropriate ones to the background thread.
  drag_out_thread_id = thread_id;
  mouse_up_received = false;
  DCHECK(!msg_hook);
  msg_hook = SetWindowsHookEx(WH_MSGFILTER,
                              MsgFilterProc,
                              NULL,
                              GetCurrentThreadId());

  // Attach the input state of the background thread to the UI thread so that
  // SetCursor can work from the background thread.
  AttachThreadInput(drag_out_thread_id, GetCurrentThreadId(), TRUE);
}

void DisableBackgroundDraggingSupport() {
  DCHECK(msg_hook);
  AttachThreadInput(drag_out_thread_id, GetCurrentThreadId(), FALSE);
  UnhookWindowsHookEx(msg_hook);
  msg_hook = NULL;
}

bool IsBackgroundDraggingSupportEnabled() {
  return msg_hook != NULL;
}

}  // namespace

class DragDropThread : public base::Thread {
 public:
  explicit DragDropThread(WebContentsDragWin* drag_handler)
       : Thread("Chrome_DragDropThread"),
         drag_handler_(drag_handler) {
  }

  virtual ~DragDropThread() {
    Stop();
  }

 protected:
  // base::Thread implementations:
  virtual void Init() {
    ole_initializer_.reset(new ui::ScopedOleInitializer());
  }

  virtual void CleanUp() {
    ole_initializer_.reset();
  }

 private:
  scoped_ptr<ui::ScopedOleInitializer> ole_initializer_;

  // Hold a reference count to WebContentsDragWin to make sure that it is always
  // alive in the thread lifetime.
  scoped_refptr<WebContentsDragWin> drag_handler_;

  DISALLOW_COPY_AND_ASSIGN(DragDropThread);
};

WebContentsDragWin::WebContentsDragWin(
    gfx::NativeWindow source_window,
    WebContents* web_contents,
    WebDragDest* drag_dest,
    const base::Callback<void()>& drag_end_callback)
    : drag_drop_thread_id_(0),
      source_window_(source_window),
      web_contents_(web_contents),
      drag_dest_(drag_dest),
      drag_ended_(false),
      drag_end_callback_(drag_end_callback) {
}

WebContentsDragWin::~WebContentsDragWin() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!drag_drop_thread_.get());
}

void WebContentsDragWin::StartDragging(const WebDropData& drop_data,
                                       WebDragOperationsMask ops,
                                       const gfx::ImageSkia& image,
                                       const gfx::Vector2d& image_offset) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  drag_source_ = new WebDragSource(source_window_, web_contents_);

  const GURL& page_url = web_contents_->GetURL();
  const std::string& page_encoding = web_contents_->GetEncoding();

  // If it is not drag-out, do the drag-and-drop in the current UI thread.
  if (drop_data.download_metadata.empty()) {
    if (DoDragging(drop_data, ops, page_url, page_encoding,
                   image, image_offset))
      EndDragging();
    return;
  }

  // Start a background thread to do the drag-and-drop.
  DCHECK(!drag_drop_thread_.get());
  drag_drop_thread_.reset(new DragDropThread(this));
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_UI;
  if (drag_drop_thread_->StartWithOptions(options)) {
    drag_drop_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&WebContentsDragWin::StartBackgroundDragging, this,
                   drop_data, ops, page_url, page_encoding,
                   image, image_offset));
  }

  EnableBackgroundDraggingSupport(drag_drop_thread_->thread_id());
}

void WebContentsDragWin::StartBackgroundDragging(
    const WebDropData& drop_data,
    WebDragOperationsMask ops,
    const GURL& page_url,
    const std::string& page_encoding,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset) {
  drag_drop_thread_id_ = base::PlatformThread::CurrentId();

  if (DoDragging(drop_data, ops, page_url, page_encoding,
                 image, image_offset)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&WebContentsDragWin::EndDragging, this));
  } else {
    // When DoDragging returns false, the contents view window is gone and thus
    // WebContentsViewWin instance becomes invalid though WebContentsDragWin
    // instance is still alive because the task holds a reference count to it.
    // We should not do more than the following cleanup items:
    // 1) Remove the background dragging support. This is safe since it does not
    //    access the instance at all.
    // 2) Stop the background thread. This is done in OnDataObjectDisposed.
    //    Only drag_drop_thread_ member is accessed.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DisableBackgroundDraggingSupport));
  }
}

void WebContentsDragWin::PrepareDragForDownload(
    const WebDropData& drop_data,
    ui::OSExchangeData* data,
    const GURL& page_url,
    const std::string& page_encoding) {
  // Parse the download metadata.
  string16 mime_type;
  base::FilePath file_name;
  GURL download_url;
  if (!ParseDownloadMetadata(drop_data.download_metadata,
                             &mime_type,
                             &file_name,
                             &download_url))
    return;

  // Generate the file name based on both mime type and proposed file name.
  std::string default_name =
      GetContentClient()->browser()->GetDefaultDownloadName();
  base::FilePath generated_download_file_name =
      net::GenerateFileName(download_url,
                            std::string(),
                            std::string(),
                            UTF16ToUTF8(file_name.value()),
                            UTF16ToUTF8(mime_type),
                            default_name);
  base::FilePath temp_dir_path;
  if (!file_util::CreateNewTempDirectory(
          FILE_PATH_LITERAL("chrome_drag"), &temp_dir_path))
    return;
  base::FilePath download_path =
      temp_dir_path.Append(generated_download_file_name);

  // We cannot know when the target application will be done using the temporary
  // file, so schedule it to be deleted after rebooting.
  file_util::DeleteAfterReboot(download_path);
  file_util::DeleteAfterReboot(temp_dir_path);

  // Provide the data as file (CF_HDROP). A temporary download file with the
  // Zone.Identifier ADS (Alternate Data Stream) attached will be created.
  scoped_refptr<DragDownloadFile> download_file =
      new DragDownloadFile(
          download_path,
          scoped_ptr<net::FileStream>(NULL),
          download_url,
          Referrer(page_url, drop_data.referrer_policy),
          page_encoding,
          web_contents_);
  ui::OSExchangeData::DownloadFileInfo file_download(base::FilePath(),
                                                     download_file.get());
  data->SetDownloadFileInfo(file_download);

  // Enable asynchronous operation.
  ui::OSExchangeDataProviderWin::GetIAsyncOperation(*data)->SetAsyncMode(TRUE);
}

void WebContentsDragWin::PrepareDragForFileContents(
    const WebDropData& drop_data, ui::OSExchangeData* data) {
  static const int kMaxFilenameLength = 255;  // FAT and NTFS
  base::FilePath file_name(drop_data.file_description_filename);

  // Images without ALT text will only have a file extension so we need to
  // synthesize one from the provided extension and URL.
  if (file_name.BaseName().RemoveExtension().empty()) {
    const string16 extension = file_name.Extension();
    // Retrieve the name from the URL.
    file_name = base::FilePath(
        net::GetSuggestedFilename(drop_data.url, "", "", "", "", ""));
    if (file_name.value().size() + extension.size() > kMaxFilenameLength) {
      file_name = base::FilePath(file_name.value().substr(
          0, kMaxFilenameLength - extension.size()));
    }
    file_name = file_name.ReplaceExtension(extension);
  }
  data->SetFileContents(file_name, drop_data.file_contents);
}

void WebContentsDragWin::PrepareDragForUrl(const WebDropData& drop_data,
                                           ui::OSExchangeData* data) {
  if (drag_dest_->delegate() &&
      drag_dest_->delegate()->AddDragData(drop_data, data)) {
    return;
  }

  data->SetURL(drop_data.url, drop_data.url_title);
}

bool WebContentsDragWin::DoDragging(const WebDropData& drop_data,
                                    WebDragOperationsMask ops,
                                    const GURL& page_url,
                                    const std::string& page_encoding,
                                    const gfx::ImageSkia& image,
                                    const gfx::Vector2d& image_offset) {
  ui::OSExchangeData data;

  if (!drop_data.download_metadata.empty()) {
    PrepareDragForDownload(drop_data, &data, page_url, page_encoding);

    // Set the observer.
    ui::OSExchangeDataProviderWin::GetDataObjectImpl(data)->set_observer(this);
  }

  // We set the file contents before the URL because the URL also sets file
  // contents (to a .URL shortcut).  We want to prefer file content data over
  // a shortcut so we add it first.
  if (!drop_data.file_contents.empty())
    PrepareDragForFileContents(drop_data, &data);
  if (!drop_data.html.string().empty())
    data.SetHtml(drop_data.html.string(), drop_data.html_base_url);
  // We set the text contents before the URL because the URL also sets text
  // content.
  if (!drop_data.text.string().empty())
    data.SetString(drop_data.text.string());
  if (drop_data.url.is_valid())
    PrepareDragForUrl(drop_data, &data);
  if (!drop_data.custom_data.empty()) {
    Pickle pickle;
    ui::WriteCustomDataToPickle(drop_data.custom_data, &pickle);
    data.SetPickledData(ui::ClipboardUtil::GetWebCustomDataFormat()->cfFormat,
                        pickle);
  }

  // Set drag image.
  if (!image.isNull()) {
    drag_utils::SetDragImageOnDataObject(image,
        gfx::Size(image.width(), image.height()), image_offset, &data);
  }

  // Use a local variable to keep track of the contents view window handle.
  // It might not be safe to access the instance after DoDragDrop returns
  // because the window could be disposed in the nested message loop.
  HWND native_window = web_contents_->GetView()->GetNativeView();

  // We need to enable recursive tasks on the message loop so we can get
  // updates while in the system DoDragDrop loop.
  DWORD effect;
  {
    // Keep a reference count such that |drag_source_| will not get deleted
    // if the contents view window is gone in the nested message loop invoked
    // from DoDragDrop.
    scoped_refptr<WebDragSource> retain_this(drag_source_);

    MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());
    DoDragDrop(ui::OSExchangeDataProviderWin::GetIDataObject(data),
               drag_source_,
               WebDragOpMaskToWinDragOpMask(ops),
               &effect);
  }

  // Bail out immediately if the contents view window is gone.
  if (!IsWindow(native_window))
    return false;

  // Normally, the drop and dragend events get dispatched in the system
  // DoDragDrop message loop so it'd be too late to set the effect to send back
  // to the renderer here. However, we use PostTask to delay the execution of
  // WebDragSource::OnDragSourceDrop, which means that the delayed dragend
  // callback to the renderer doesn't run until this has been set to the correct
  // value.
  drag_source_->set_effect(effect);

  return true;
}

void WebContentsDragWin::EndDragging() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (drag_ended_)
    return;
  drag_ended_ = true;

  if (IsBackgroundDraggingSupportEnabled())
    DisableBackgroundDraggingSupport();

  drag_end_callback_.Run();
}

void WebContentsDragWin::CancelDrag() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  drag_source_->CancelDrag();
}

void WebContentsDragWin::CloseThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  drag_drop_thread_.reset();
}

void WebContentsDragWin::OnWaitForData() {
  DCHECK(drag_drop_thread_id_ == base::PlatformThread::CurrentId());

  // When the left button is release and we start to wait for the data, end
  // the dragging before DoDragDrop returns. This makes the page leave the drag
  // mode so that it can start to process the normal input events.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WebContentsDragWin::EndDragging, this));
}

void WebContentsDragWin::OnDataObjectDisposed() {
  DCHECK(drag_drop_thread_id_ == base::PlatformThread::CurrentId());

  // The drag-and-drop thread is only closed after OLE is done with
  // DataObjectImpl.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WebContentsDragWin::CloseThread, this));
}

}  // namespace content
