// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/html_viewer/html_document_view.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/services/html_viewer/blink_input_events_type_converters.h"
#include "mojo/services/html_viewer/webstoragenamespace_impl.h"
#include "mojo/services/html_viewer/weburlloader_impl.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace mojo {
namespace {

// Ripped from web_url_loader_impl.cc. Why is everything so complicated?
class HeaderFlattener : public blink::WebHTTPHeaderVisitor {
 public:
  HeaderFlattener() : has_accept_header_(false) {}

  virtual void visitHeader(const blink::WebString& name,
                           const blink::WebString& value) {
    // Headers are latin1.
    const std::string& name_latin1 = name.latin1();
    const std::string& value_latin1 = value.latin1();

    // Skip over referrer headers found in the header map because we already
    // pulled it out as a separate parameter.
    if (LowerCaseEqualsASCII(name_latin1, "referer"))
      return;

    if (LowerCaseEqualsASCII(name_latin1, "accept"))
      has_accept_header_ = true;

    buffer_.push_back(name_latin1 + ": " + value_latin1);
  }

  Array<String> GetBuffer() {
    // In some cases, WebKit doesn't add an Accept header, but not having the
    // header confuses some web servers.  See bug 808613.
    if (!has_accept_header_) {
      buffer_.push_back("Accept: */*");
      has_accept_header_ = true;
    }
    return buffer_.Pass();
  }

 private:
  Array<String> buffer_;
  bool has_accept_header_;
};

void AddRequestBody(NavigationDetails* nav_details,
                    const blink::WebURLRequest& request) {
  if (request.httpBody().isNull())
    return;

  uint32_t i = 0;
  blink::WebHTTPBody::Element element;
  while (request.httpBody().elementAt(i++, element)) {
    switch (element.type) {
      case blink::WebHTTPBody::Element::TypeData:
        if (!element.data.isEmpty()) {
          // WebKit sometimes gives up empty data to append. These aren't
          // necessary so we just optimize those out here.
          uint32_t num_bytes = static_cast<uint32_t>(element.data.size());
          MojoCreateDataPipeOptions options;
          options.struct_size = sizeof(MojoCreateDataPipeOptions);
          options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
          options.element_num_bytes = 1;
          options.capacity_num_bytes = num_bytes;
          DataPipe data_pipe(options);
          nav_details->request->body.push_back(
              data_pipe.consumer_handle.Pass());
          WriteDataRaw(data_pipe.producer_handle.get(),
                       element.data.data(),
                       &num_bytes,
                       MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
        }
        break;
      case blink::WebHTTPBody::Element::TypeFile:
      case blink::WebHTTPBody::Element::TypeFileSystemURL:
      case blink::WebHTTPBody::Element::TypeBlob:
        // TODO(mpcomplete): handle these.
        NOTIMPLEMENTED();
        break;
      default:
        NOTREACHED();
    }
  }
}

void ConfigureSettings(blink::WebSettings* settings) {
  settings->setAcceleratedCompositingEnabled(false);
  settings->setCookieEnabled(true);
  settings->setDefaultFixedFontSize(13);
  settings->setDefaultFontSize(16);
  settings->setLoadsImagesAutomatically(true);
  settings->setJavaScriptEnabled(true);
}

Target WebNavigationPolicyToNavigationTarget(
    blink::WebNavigationPolicy policy) {
  switch (policy) {
    case blink::WebNavigationPolicyCurrentTab:
      return TARGET_SOURCE_NODE;
    case blink::WebNavigationPolicyNewBackgroundTab:
    case blink::WebNavigationPolicyNewForegroundTab:
    case blink::WebNavigationPolicyNewWindow:
    case blink::WebNavigationPolicyNewPopup:
      return TARGET_NEW_NODE;
    default:
      return TARGET_DEFAULT;
  }
}

bool CanNavigateLocally(blink::WebFrame* frame,
                        const blink::WebURLRequest& request) {
  // For now, we just load child frames locally.
  // TODO(aa): In the future, this should use embedding to connect to a
  // different instance of Blink if the frame is cross-origin.
  if (frame->parent())
    return true;

  // If we have extraData() it means we already have the url response
  // (presumably because we are being called via Navigate()). In that case we
  // can go ahead and navigate locally.
  if (request.extraData())
    return true;

  // Otherwise we don't know if we're the right app to handle this request. Ask
  // host to do the navigation for us.
  return false;
}

}  // namespace

HTMLDocumentView::HTMLDocumentView(ServiceProvider* service_provider,
                                   ViewManager* view_manager)
    : view_manager_(view_manager),
      web_view_(NULL),
      root_(NULL),
      repaint_pending_(false),
      navigator_host_(service_provider),
      weak_factory_(this) {
}

HTMLDocumentView::~HTMLDocumentView() {
  if (web_view_)
    web_view_->close();
  if (root_)
    root_->RemoveObserver(this);
}

void HTMLDocumentView::AttachToNode(Node* node) {
  root_ = node;
  root_->SetColor(SK_ColorCYAN);  // Dummy background color.

  web_view_ = blink::WebView::create(this);
  ConfigureSettings(web_view_->settings());
  web_view_->setMainFrame(blink::WebLocalFrame::create(this));
  web_view_->resize(root_->bounds().size());

  root_->AddObserver(this);
}

void HTMLDocumentView::Load(URLResponsePtr response) {
  DCHECK(web_view_);

  GURL url(response->url);

  WebURLRequestExtraData* extra_data = new WebURLRequestExtraData;
  extra_data->synthetic_response = response.Pass();

  blink::WebURLRequest web_request;
  web_request.initialize();
  web_request.setURL(url);
  web_request.setExtraData(extra_data);

  web_view_->mainFrame()->loadRequest(web_request);
}

blink::WebStorageNamespace* HTMLDocumentView::createSessionStorageNamespace() {
  return new WebStorageNamespaceImpl();
}

void HTMLDocumentView::didInvalidateRect(const blink::WebRect& rect) {
  if (!repaint_pending_) {
    repaint_pending_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&HTMLDocumentView::Repaint, weak_factory_.GetWeakPtr()));
  }
}

bool HTMLDocumentView::allowsBrokenNullLayerTreeView() const {
  // TODO(darin): Switch to using compositor bindings.
  //
  // NOTE: Note to Blink maintainers, feel free to break this code if it is the
  // last NOT using compositor bindings and you want to delete this code path.
  //
  return true;
}

blink::WebCookieJar* HTMLDocumentView::cookieJar(blink::WebLocalFrame* frame) {
  // TODO(darin): Blink does not fallback to the Platform provided WebCookieJar.
  // Either it should, as it once did, or we should find another solution here.
  return blink::Platform::current()->cookieJar();
}

blink::WebNavigationPolicy HTMLDocumentView::decidePolicyForNavigation(
    blink::WebLocalFrame* frame, blink::WebDataSource::ExtraData* data,
    const blink::WebURLRequest& request, blink::WebNavigationType nav_type,
    blink::WebNavigationPolicy default_policy, bool is_redirect) {
  if (CanNavigateLocally(frame, request))
    return default_policy;

  NavigationDetailsPtr nav_details(NavigationDetails::New());
  nav_details->request->url = request.url().string().utf8();
  nav_details->request->method = request.httpMethod().utf8();

  HeaderFlattener flattener;
  request.visitHTTPHeaderFields(&flattener);
  nav_details->request->headers = flattener.GetBuffer().Pass();

  AddRequestBody(nav_details.get(), request);

  navigator_host_->RequestNavigate(
      root_->id(),
      WebNavigationPolicyToNavigationTarget(default_policy),
      nav_details.Pass());

  return blink::WebNavigationPolicyIgnore;
}

void HTMLDocumentView::didAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line,
    const blink::WebString& stack_trace) {
}

void HTMLDocumentView::didNavigateWithinPage(
    blink::WebLocalFrame* frame, const blink::WebHistoryItem& history_item,
    blink::WebHistoryCommitType commit_type) {
  navigator_host_->DidNavigateLocally(root_->id(),
                                      history_item.urlString().utf8());
}

void HTMLDocumentView::OnNodeBoundsChanged(Node* node,
                                           const gfx::Rect& old_bounds,
                                           const gfx::Rect& new_bounds) {
  DCHECK_EQ(node, root_);
  web_view_->resize(node->bounds().size());
}

void HTMLDocumentView::OnNodeDestroyed(Node* node) {
  DCHECK_EQ(node, root_);
  node->RemoveObserver(this);
  root_ = NULL;
}

void HTMLDocumentView::OnNodeInputEvent(Node* node, const EventPtr& event) {
  scoped_ptr<blink::WebInputEvent> web_event =
      TypeConverter<EventPtr, scoped_ptr<blink::WebInputEvent> >::ConvertTo(
          event);
  if (web_event)
    web_view_->handleInputEvent(*web_event);
}

void HTMLDocumentView::Repaint() {
  repaint_pending_ = false;

  web_view_->animate(0.0);
  web_view_->layout();

  int width = web_view_->size().width;
  int height = web_view_->size().height;

  skia::RefPtr<SkCanvas> canvas = skia::AdoptRef(SkCanvas::NewRaster(
      SkImageInfo::MakeN32(width, height, kOpaque_SkAlphaType)));

  web_view_->paint(canvas.get(), gfx::Rect(0, 0, width, height));

  root_->SetContents(canvas->getDevice()->accessBitmap(false));
}

}  // namespace mojo
