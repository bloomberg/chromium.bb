// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRINT_WEB_VIEW_HELPER_H_
#define CHROME_RENDERER_PRINT_WEB_VIEW_HELPER_H_

#include <vector>

#include "base/gfx/size.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "webkit/glue/webview_delegate.h"

namespace gfx {
class Size;
}

namespace IPC {
class Message;
}

#if defined(OS_LINUX)
namespace printing {
class PdfPsMetafile;
typedef PdfPsMetafile NativeMetafile;
}
#endif

class RenderView;
class WebView;
struct ViewMsg_Print_Params;
struct ViewMsg_PrintPage_Params;
struct ViewMsg_PrintPages_Params;


// Class that calls the Begin and End print functions on the frame and changes
// the size of the view temporarily to support full page printing..
// Do not serve any events in the time between construction and destruction of
// this class because it will cause flicker.
class PrepareFrameAndViewForPrint {
 public:
  PrepareFrameAndViewForPrint(const ViewMsg_Print_Params& print_params,
                              WebKit::WebFrame* frame,
                              WebView* web_view);
  ~PrepareFrameAndViewForPrint();

  int GetExpectedPageCount() const {
    return expected_pages_count_;
  }

  const gfx::Size& GetPrintCanvasSize() const {
    return print_canvas_size_;
  }

 private:
  WebKit::WebFrame* frame_;
  WebView* web_view_;
  gfx::Size print_canvas_size_;
  gfx::Size prev_view_size_;
  int expected_pages_count_;

  DISALLOW_COPY_AND_ASSIGN(PrepareFrameAndViewForPrint);
};


// PrintWebViewHelper handles most of the printing grunt work for RenderView.
// We plan on making print asynchronous and that will require copying the DOM
// of the document and creating a new WebView with the contents.
class PrintWebViewHelper : public WebViewDelegate {
 public:
  explicit PrintWebViewHelper(RenderView* render_view);
  virtual ~PrintWebViewHelper();

  void Print(WebKit::WebFrame* frame, bool script_initiated);

  // Is there a background print in progress?
  bool IsPrinting() {
    return print_web_view_.get() != NULL;
  }

  // Notification when printing is done - signal teardown
  void DidFinishPrinting(bool success);

  // Prints the page listed in |params| as a JPEG image. The width and height of
  // the image will scale propotionally given the |zoom_factor| multiplier. The
  // encoded JPEG data will be written into the supplied vector |image_data|.
  void PrintPageAsJPEG(const ViewMsg_PrintPage_Params& params,
                       WebKit::WebFrame* frame,
                       float zoom_factor,
                       std::vector<unsigned char>* image_data);

 protected:
  bool CopyAndPrint(const ViewMsg_PrintPages_Params& params,
                    WebKit::WebFrame* web_frame);

  // Prints the page listed in |params|.
#if defined(OS_LINUX)
  void PrintPage(const ViewMsg_PrintPage_Params& params,
                 const gfx::Size& canvas_size,
                 WebKit::WebFrame* frame,
                 printing::NativeMetafile* metafile);
#else
  void PrintPage(const ViewMsg_PrintPage_Params& params,
                 const gfx::Size& canvas_size,
                 WebKit::WebFrame* frame);
#endif

  // Prints all the pages listed in |params|.
  // It will implicitly revert the document to display CSS media type.
  void PrintPages(const ViewMsg_PrintPages_Params& params,
                  WebKit::WebFrame* frame);

  // IPC::Message::Sender
  bool Send(IPC::Message* msg);

  int32 routing_id();

  // WebKit::WebViewClient
  virtual WebView* createView(WebKit::WebFrame* creator) { return NULL; }
  virtual WebKit::WebWidget* createPopupMenu(bool activatable) { return NULL; }
  virtual WebKit::WebWidget* createPopupMenu(
      const WebKit::WebPopupMenuInfo& info) { return NULL; }
  virtual void didAddMessageToConsole(
      const WebKit::WebConsoleMessage& message,
      const WebKit::WebString& source_name, unsigned source_line) {}
  virtual void printPage(WebKit::WebFrame* frame) {}
  virtual WebKit::WebNotificationPresenter* notificationPresenter() {
    return NULL;
  }
  virtual void didStartLoading() {}
  virtual void didStopLoading();
  virtual bool shouldBeginEditing(const WebKit::WebRange& range) {
    return false;
  }
  virtual bool shouldEndEditing(const WebKit::WebRange& range) {
    return false;
  }
  virtual bool shouldInsertNode(
      const WebKit::WebNode& node, const WebKit::WebRange& range,
      WebKit::WebEditingAction action) { return false; }
  virtual bool shouldInsertText(
      const WebKit::WebString& text, const WebKit::WebRange& range,
      WebKit::WebEditingAction action) { return false; }
  virtual bool shouldChangeSelectedRange(
      const WebKit::WebRange& from, const WebKit::WebRange& to,
      WebKit::WebTextAffinity affinity, bool still_selecting) { return false; }
  virtual bool shouldDeleteRange(const WebKit::WebRange& range) {
    return false;
  }
  virtual bool shouldApplyStyle(
      const WebKit::WebString& style, const WebKit::WebRange& range) {
    return false;
  }
  virtual bool isSmartInsertDeleteEnabled() { return false; }
  virtual bool isSelectTrailingWhitespaceEnabled() { return false; }
  virtual void setInputMethodEnabled(bool enabled) {}
  virtual void didBeginEditing() {}
  virtual void didChangeSelection(bool is_selection_empty) {}
  virtual void didChangeContents() {}
  virtual void didExecuteCommand(const WebKit::WebString& command_name) {}
  virtual void didEndEditing() {}
  virtual bool handleCurrentKeyboardEvent() { return false; }
  virtual void spellCheck(
      const WebKit::WebString& text, int& offset, int& length) {}
  virtual WebKit::WebString autoCorrectWord(
      const WebKit::WebString& misspelled_word);
  virtual void showSpellingUI(bool show) {}
  virtual bool isShowingSpellingUI() { return false; }
  virtual void updateSpellingUIWithMisspelledWord(
      const WebKit::WebString& word) {}
  virtual bool runFileChooser(
      bool multi_select, const WebKit::WebString& title,
      const WebKit::WebString& initial_value,
      WebKit::WebFileChooserCompletion* chooser_completion) {
    return false;
  }
  virtual void runModalAlertDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message) {}
  virtual bool runModalConfirmDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message) {
    return false;
  }
  virtual bool runModalPromptDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message,
      const WebKit::WebString& default_value,
      WebKit::WebString* actual_value) { return false; }
  virtual bool runModalBeforeUnloadDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message) {
    return true;
  }
  virtual void showContextMenu(
      WebKit::WebFrame* frame, const WebKit::WebContextMenuData& data) {}
  virtual void setStatusText(const WebKit::WebString& text) {}
  virtual void setMouseOverURL(const WebKit::WebURL& url) {}
  virtual void setToolTipText(
      const WebKit::WebString& text, WebKit::WebTextDirection hint) {}
  virtual void startDragging(
      const WebKit::WebPoint& from, const WebKit::WebDragData& data,
      WebKit::WebDragOperationsMask mask) {}
  virtual bool acceptsLoadDrops() { return false; }
  virtual void focusNext() {}
  virtual void focusPrevious() {}
  virtual void navigateBackForwardSoon(int offset) {}
  virtual int historyBackListCount() { return 0; }
  virtual int historyForwardListCount() { return 0; }
  virtual void didAddHistoryItem() {}
  virtual void focusAccessibilityObject(
      const WebKit::WebAccessibilityObject& object) {}
  virtual void didUpdateInspectorSettings() {}
  virtual WebKit::WebDevToolsAgentClient* devToolsAgentClient() {
    return NULL;
  }
  virtual void queryAutofillSuggestions(
      const WebKit::WebNode& node, const WebKit::WebString& name,
      const WebKit::WebString& value) {}
  virtual void removeAutofillSuggestions(
      const WebKit::WebString& name, const WebKit::WebString& value) {}

  // WebKit::WebWidgetClient
  virtual void didInvalidateRect(const WebKit::WebRect&) {}
  virtual void didScrollRect(
      int dx, int dy, const WebKit::WebRect& clipRect) {}
  virtual void didFocus() {}
  virtual void didBlur() {}
  virtual void didChangeCursor(const WebKit::WebCursorInfo&) {}
  virtual void closeWidgetSoon() {}
  virtual void show(WebKit::WebNavigationPolicy) {}
  virtual void runModal() {}
  virtual WebKit::WebRect windowRect();
  virtual void setWindowRect(const WebKit::WebRect&) {}
  virtual WebKit::WebRect windowResizerRect();
  virtual WebKit::WebRect rootWindowRect();
  virtual WebKit::WebScreenInfo screenInfo();

 private:
  RenderView* render_view_;
  scoped_ptr<WebView> print_web_view_;
  scoped_ptr<ViewMsg_PrintPages_Params> print_pages_params_;
  base::Time last_cancelled_script_print_;
  int user_cancelled_scripted_print_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintWebViewHelper);
};

#endif  // CHROME_RENDERER_PRINT_WEB_VIEW_HELPER_H_
