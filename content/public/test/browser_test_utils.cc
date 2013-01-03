// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test_utils.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/net_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/test/test_utils.h"
#include "net/cookies/cookie_store.h"
#include "net/test/python_utils.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

static const int kDefaultWsPort = 8880;

namespace content {
namespace {

class DOMOperationObserver : public NotificationObserver,
                             public WebContentsObserver {
 public:
  explicit DOMOperationObserver(RenderViewHost* render_view_host)
      : WebContentsObserver(WebContents::FromRenderViewHost(render_view_host)),
        did_respond_(false) {
    registrar_.Add(this, NOTIFICATION_DOM_OPERATION_RESPONSE,
                   Source<RenderViewHost>(render_view_host));
    message_loop_runner_ = new MessageLoopRunner;
  }

  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE {
    DCHECK(type == NOTIFICATION_DOM_OPERATION_RESPONSE);
    Details<DomOperationNotificationDetails> dom_op_details(details);
    response_ = dom_op_details->json;
    did_respond_ = true;
    message_loop_runner_->Quit();
  }

  // Overridden from WebContentsObserver:
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE {
    message_loop_runner_->Quit();
  }

  bool WaitAndGetResponse(std::string* response) WARN_UNUSED_RESULT {
    message_loop_runner_->Run();
    *response = response_;
    return did_respond_;
  }

 private:
  NotificationRegistrar registrar_;
  std::string response_;
  bool did_respond_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(DOMOperationObserver);
};

// Specifying a prototype so that we can add the WARN_UNUSED_RESULT attribute.
bool ExecuteJavaScriptHelper(RenderViewHost* render_view_host,
                             const std::string& frame_xpath,
                             const std::string& original_script,
                             scoped_ptr<Value>* result) WARN_UNUSED_RESULT;

// Executes the passed |original_script| in the frame pointed to by
// |frame_xpath|.  If |result| is not NULL, stores the value that the evaluation
// of the script in |result|.  Returns true on success.
bool ExecuteJavaScriptHelper(RenderViewHost* render_view_host,
                             const std::string& frame_xpath,
                             const std::string& original_script,
                             scoped_ptr<Value>* result) {
  // TODO(jcampan): we should make the domAutomationController not require an
  //                automation id.
  std::string script =
      "window.domAutomationController.setAutomationId(0);" + original_script;
  DOMOperationObserver dom_op_observer(render_view_host);
  render_view_host->ExecuteJavascriptInWebFrame(UTF8ToUTF16(frame_xpath),
                                                UTF8ToUTF16(script));
  std::string json;
  if (!dom_op_observer.WaitAndGetResponse(&json)) {
    DLOG(ERROR) << "Cannot communicate with DOMOperationObserver.";
    return false;
  }

  // Nothing more to do for callers that ignore the returned JS value.
  if (!result)
    return true;

  base::JSONReader reader(base::JSON_ALLOW_TRAILING_COMMAS);
  result->reset(reader.ReadToValue(json));
  if (!result->get()) {
    DLOG(ERROR) << reader.GetErrorMessage();
    return false;
  }

  return true;
}

void BuildSimpleWebKeyEvent(WebKit::WebInputEvent::Type type,
                            ui::KeyboardCode key,
                            bool control,
                            bool shift,
                            bool alt,
                            bool command,
                            NativeWebKeyboardEvent* event) {
  event->nativeKeyCode = 0;
  event->windowsKeyCode = key;
  event->setKeyIdentifierFromWindowsKeyCode();
  event->type = type;
  event->modifiers = 0;
  event->isSystemKey = false;
  event->timeStampSeconds = base::Time::Now().ToDoubleT();
  event->skip_in_browser = true;

  if (type == WebKit::WebInputEvent::Char ||
      type == WebKit::WebInputEvent::RawKeyDown) {
    event->text[0] = key;
    event->unmodifiedText[0] = key;
  }

  if (control)
    event->modifiers |= WebKit::WebInputEvent::ControlKey;

  if (shift)
    event->modifiers |= WebKit::WebInputEvent::ShiftKey;

  if (alt)
    event->modifiers |= WebKit::WebInputEvent::AltKey;

  if (command)
    event->modifiers |= WebKit::WebInputEvent::MetaKey;
}

void GetCookiesCallback(std::string* cookies_out,
                        base::WaitableEvent* event,
                        const std::string& cookies) {
  *cookies_out = cookies;
  event->Signal();
}

void GetCookiesOnIOThread(const GURL& url,
                          net::URLRequestContextGetter* context_getter,
                          base::WaitableEvent* event,
                          std::string* cookies) {
  net::CookieStore* cookie_store =
      context_getter->GetURLRequestContext()->cookie_store();
  cookie_store->GetCookiesWithOptionsAsync(
      url, net::CookieOptions(),
      base::Bind(&GetCookiesCallback, cookies, event));
}

void SetCookieCallback(bool* result,
                       base::WaitableEvent* event,
                       bool success) {
  *result = success;
  event->Signal();
}

void SetCookieOnIOThread(const GURL& url,
                         const std::string& value,
                         net::URLRequestContextGetter* context_getter,
                         base::WaitableEvent* event,
                         bool* result) {
  net::CookieStore* cookie_store =
      context_getter->GetURLRequestContext()->cookie_store();
  cookie_store->SetCookieWithOptionsAsync(
      url, value, net::CookieOptions(),
      base::Bind(&SetCookieCallback, result, event));
}

}  // namespace


GURL GetFileUrlWithQuery(const FilePath& path,
                         const std::string& query_string) {
  GURL url = net::FilePathToFileURL(path);
  if (!query_string.empty()) {
    GURL::Replacements replacements;
    replacements.SetQueryStr(query_string);
    return url.ReplaceComponents(replacements);
  }
  return url;
}

void WaitForLoadStop(WebContents* web_contents) {
    WindowedNotificationObserver load_stop_observer(
    NOTIFICATION_LOAD_STOP,
    Source<NavigationController>(&web_contents->GetController()));
  // In many cases, the load may have finished before we get here.  Only wait if
  // the tab still has a pending navigation.
  if (!web_contents->IsLoading())
    return;
  load_stop_observer.Wait();
}

void CrashTab(WebContents* web_contents) {
  RenderProcessHost* rph = web_contents->GetRenderProcessHost();
  WindowedNotificationObserver observer(
      NOTIFICATION_RENDERER_PROCESS_CLOSED,
      Source<RenderProcessHost>(rph));
  base::KillProcess(rph->GetHandle(), 0, false);
  observer.Wait();
}

void SimulateMouseClick(WebContents* web_contents,
                        int modifiers,
                        WebKit::WebMouseEvent::Button button) {
  int x = web_contents->GetView()->GetContainerSize().width() / 2;
  int y = web_contents->GetView()->GetContainerSize().height() / 2;
  WebKit::WebMouseEvent mouse_event;
  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.button = button;
  mouse_event.x = x;
  mouse_event.y = y;
  mouse_event.modifiers = modifiers;
  // Mac needs globalX/globalY for events to plugins.
  gfx::Rect offset;
  web_contents->GetView()->GetContainerBounds(&offset);
  mouse_event.globalX = x + offset.x();
  mouse_event.globalY = y + offset.y();
  mouse_event.clickCount = 1;
  web_contents->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  web_contents->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
}

void SimulateMouseEvent(WebContents* web_contents,
                        WebKit::WebInputEvent::Type type,
                        const gfx::Point& point) {
  WebKit::WebMouseEvent mouse_event;
  mouse_event.type = type;
  mouse_event.x = point.x();
  mouse_event.y = point.y();
  web_contents->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
}

void SimulateKeyPress(WebContents* web_contents,
                      ui::KeyboardCode key,
                      bool control,
                      bool shift,
                      bool alt,
                      bool command) {
  NativeWebKeyboardEvent event_down;
  BuildSimpleWebKeyEvent(
      WebKit::WebInputEvent::RawKeyDown, key, control, shift, alt, command,
      &event_down);
  web_contents->GetRenderViewHost()->ForwardKeyboardEvent(event_down);

  NativeWebKeyboardEvent char_event;
  BuildSimpleWebKeyEvent(
      WebKit::WebInputEvent::Char, key, control, shift, alt, command,
      &char_event);
  web_contents->GetRenderViewHost()->ForwardKeyboardEvent(char_event);

  NativeWebKeyboardEvent event_up;
  BuildSimpleWebKeyEvent(
      WebKit::WebInputEvent::KeyUp, key, control, shift, alt, command,
      &event_up);
  web_contents->GetRenderViewHost()->ForwardKeyboardEvent(event_up);
}

bool ExecuteJavaScript(RenderViewHost* render_view_host,
                       const std::string& frame_xpath,
                       const std::string& original_script) {
  std::string script =
      original_script + ";window.domAutomationController.send(0);";
  return ExecuteJavaScriptHelper(render_view_host, frame_xpath, script, NULL);
}

bool ExecuteJavaScriptAndExtractInt(RenderViewHost* render_view_host,
                                    const std::string& frame_xpath,
                                    const std::string& script,
                                    int* result) {
  DCHECK(result);
  scoped_ptr<Value> value;
  if (!ExecuteJavaScriptHelper(render_view_host, frame_xpath, script, &value) ||
      !value.get())
    return false;

  return value->GetAsInteger(result);
}

bool ExecuteJavaScriptAndExtractBool(RenderViewHost* render_view_host,
                                     const std::string& frame_xpath,
                                     const std::string& script,
                                     bool* result) {
  DCHECK(result);
  scoped_ptr<Value> value;
  if (!ExecuteJavaScriptHelper(render_view_host, frame_xpath, script, &value) ||
      !value.get())
    return false;

  return value->GetAsBoolean(result);
}

bool ExecuteJavaScriptAndExtractString(RenderViewHost* render_view_host,
                                       const std::string& frame_xpath,
                                       const std::string& script,
                                       std::string* result) {
  DCHECK(result);
  scoped_ptr<Value> value;
  if (!ExecuteJavaScriptHelper(render_view_host, frame_xpath, script, &value) ||
      !value.get())
    return false;

  return value->GetAsString(result);
}

std::string GetCookies(BrowserContext* browser_context, const GURL& url) {
  std::string cookies;
  base::WaitableEvent event(true, false);
  net::URLRequestContextGetter* context_getter =
      browser_context->GetRequestContext();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&GetCookiesOnIOThread, url,
                 make_scoped_refptr(context_getter), &event, &cookies));
  event.Wait();
  return cookies;
}

bool SetCookie(BrowserContext* browser_context,
               const GURL& url,
               const std::string& value) {
  bool result = false;
  base::WaitableEvent event(true, false);
  net::URLRequestContextGetter* context_getter =
      browser_context->GetRequestContext();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SetCookieOnIOThread, url, value,
                 make_scoped_refptr(context_getter), &event, &result));
  event.Wait();
  return result;
}

TitleWatcher::TitleWatcher(WebContents* web_contents,
                           const string16& expected_title)
    : web_contents_(web_contents),
      expected_title_observed_(false),
      quit_loop_on_observation_(false) {
  EXPECT_TRUE(web_contents != NULL);
  expected_titles_.push_back(expected_title);
  notification_registrar_.Add(this,
                              NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED,
                              Source<WebContents>(web_contents));

  // When navigating through the history, the restored NavigationEntry's title
  // will be used. If the entry ends up having the same title after we return
  // to it, as will usually be the case, the
  // NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED will then be suppressed, since the
  // NavigationEntry's title hasn't changed.
  notification_registrar_.Add(
      this,
      NOTIFICATION_LOAD_STOP,
      Source<NavigationController>(&web_contents->GetController()));
}

void TitleWatcher::AlsoWaitForTitle(const string16& expected_title) {
  expected_titles_.push_back(expected_title);
}

TitleWatcher::~TitleWatcher() {
}

const string16& TitleWatcher::WaitAndGetTitle() {
  if (expected_title_observed_)
    return observed_title_;
  quit_loop_on_observation_ = true;
  message_loop_runner_ = new MessageLoopRunner;
  message_loop_runner_->Run();
  return observed_title_;
}

void TitleWatcher::Observe(int type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  if (type == NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED) {
    WebContents* source_contents = Source<WebContents>(source).ptr();
    ASSERT_EQ(web_contents_, source_contents);
  } else if (type == NOTIFICATION_LOAD_STOP) {
    NavigationController* controller =
        Source<NavigationController>(source).ptr();
    ASSERT_EQ(&web_contents_->GetController(), controller);
  } else {
    FAIL() << "Unexpected notification received.";
  }

  std::vector<string16>::const_iterator it =
      std::find(expected_titles_.begin(),
                expected_titles_.end(),
                web_contents_->GetTitle());
  if (it == expected_titles_.end())
    return;
  observed_title_ = *it;
  expected_title_observed_ = true;
  if (quit_loop_on_observation_) {
    // Only call Quit once, on first Observe:
    quit_loop_on_observation_ = false;
    message_loop_runner_->Quit();
  }
}

DOMMessageQueue::DOMMessageQueue() : waiting_for_message_(false) {
  registrar_.Add(this, NOTIFICATION_DOM_OPERATION_RESPONSE,
                 NotificationService::AllSources());
}

DOMMessageQueue::~DOMMessageQueue() {}

void DOMMessageQueue::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  Details<DomOperationNotificationDetails> dom_op_details(details);
  Source<RenderViewHost> sender(source);
  message_queue_.push(dom_op_details->json);
  if (waiting_for_message_) {
    waiting_for_message_ = false;
    message_loop_runner_->Quit();
  }
}

void DOMMessageQueue::ClearQueue() {
  message_queue_ = std::queue<std::string>();
}

bool DOMMessageQueue::WaitForMessage(std::string* message) {
  if (message_queue_.empty()) {
    waiting_for_message_ = true;
    // This will be quit when a new message comes in.
    message_loop_runner_ = new MessageLoopRunner;
    message_loop_runner_->Run();
  }
  // The queue should not be empty, unless we were quit because of a timeout.
  if (message_queue_.empty())
    return false;
  if (message)
    *message = message_queue_.front();
  return true;
}

}  // namespace content
