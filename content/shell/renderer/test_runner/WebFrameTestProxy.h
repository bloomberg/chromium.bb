// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBFRAMETESTPROXY_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBFRAMETESTPROXY_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/test_runner.h"
#include "content/shell/renderer/test_runner/TestInterfaces.h"
#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "content/shell/renderer/test_runner/WebTestProxy.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace WebTestRunner {

// Templetized wrapper around RenderFrameImpl objects, which implement
// the WebFrameClient interface.
template<class Base, typename P, typename R>
class WebFrameTestProxy : public Base {
public:
    WebFrameTestProxy(P p, R r)
        : Base(p, r)
        , m_baseProxy(0)
        , m_version(0) { }

    virtual ~WebFrameTestProxy() { }

    void setBaseProxy(WebTestProxyBase* proxy)
    {
        m_baseProxy = proxy;
    }

    void setVersion(int version)
    {
        m_version = version;
    }

    blink::WebPlugin* createPlugin(blink::WebLocalFrame* frame, const blink::WebPluginParams& params)
    {
        blink::WebPlugin* plugin = m_baseProxy->createPlugin(frame, params);
        if (plugin)
            return plugin;
        return Base::createPlugin(frame, params);
    }

    // WebFrameClient implementation.
    virtual void didAddMessageToConsole(const blink::WebConsoleMessage& message, const blink::WebString& sourceName, unsigned sourceLine, const blink::WebString& stackTrace)
    {
        m_baseProxy->didAddMessageToConsole(message, sourceName, sourceLine);
        Base::didAddMessageToConsole(message, sourceName, sourceLine, stackTrace);
    }
    virtual bool canCreatePluginWithoutRenderer(const blink::WebString& mimeType)
    {
        using blink::WebString;

        const CR_DEFINE_STATIC_LOCAL(WebString, suffix, ("-can-create-without-renderer"));
        return mimeType.utf8().find(suffix.utf8()) != std::string::npos;
    }
    virtual void didStartProvisionalLoad(blink::WebLocalFrame* frame)
    {
        if (m_version > 2)
            m_baseProxy->didStartProvisionalLoad(frame);
        Base::didStartProvisionalLoad(frame);
    }
    virtual void didReceiveServerRedirectForProvisionalLoad(blink::WebLocalFrame* frame)
    {
        m_baseProxy->didReceiveServerRedirectForProvisionalLoad(frame);
        Base::didReceiveServerRedirectForProvisionalLoad(frame);
    }
    virtual void didFailProvisionalLoad(blink::WebLocalFrame* frame, const blink::WebURLError& error)
    {
        Base::didFailProvisionalLoad(frame, error);
    }
    virtual void didCommitProvisionalLoad(blink::WebLocalFrame* frame, bool isNewNavigation)
    {
        m_baseProxy->didCommitProvisionalLoad(frame, isNewNavigation);
        Base::didCommitProvisionalLoad(frame, isNewNavigation);
    }
    virtual void didReceiveTitle(blink::WebLocalFrame* frame, const blink::WebString& title, blink::WebTextDirection direction)
    {
        Base::didReceiveTitle(frame, title, direction);
    }
    virtual void didChangeIcon(blink::WebLocalFrame* frame, blink::WebIconURL::Type iconType)
    {
        Base::didChangeIcon(frame, iconType);
    }
    virtual void didFinishDocumentLoad(blink::WebLocalFrame* frame)
    {
        Base::didFinishDocumentLoad(frame);
    }
    virtual void didHandleOnloadEvents(blink::WebLocalFrame* frame)
    {
        Base::didHandleOnloadEvents(frame);
    }
    virtual void didFailLoad(blink::WebLocalFrame* frame, const blink::WebURLError& error)
    {
        Base::didFailLoad(frame, error);
    }
    virtual void didFinishLoad(blink::WebLocalFrame* frame)
    {
        Base::didFinishLoad(frame);
    }
    virtual void didChangeSelection(bool is_selection_empty) {
        m_baseProxy->didChangeSelection(is_selection_empty);
        Base::didChangeSelection(is_selection_empty);
    }
    virtual blink::WebColorChooser* createColorChooser(
        blink::WebColorChooserClient* client,
        const blink::WebColor& initial_color,
        const blink::WebVector<blink::WebColorSuggestion>& suggestions) {
      return m_baseProxy->createColorChooser(client, initial_color, suggestions);
    }
    virtual void runModalAlertDialog(const blink::WebString& message) {
        m_baseProxy->m_delegate->printMessage(std::string("ALERT: ") + message.utf8().data() + "\n");
    }
    virtual bool runModalConfirmDialog(const blink::WebString& message) {
        m_baseProxy->m_delegate->printMessage(std::string("CONFIRM: ") + message.utf8().data() + "\n");
        return true;
    }
    virtual bool runModalPromptDialog(const blink::WebString& message, const blink::WebString& defaultValue, blink::WebString*) {
        m_baseProxy->m_delegate->printMessage(std::string("PROMPT: ") + message.utf8().data() + ", default text: " + defaultValue.utf8().data() + "\n");
        return true;
    }
    virtual bool runModalBeforeUnloadDialog(bool is_reload, const blink::WebString& message) {
        m_baseProxy->m_delegate->printMessage(std::string("CONFIRM NAVIGATION: ") + message.utf8().data() + "\n");
        return !m_baseProxy->m_testInterfaces->testRunner()->shouldStayOnPageAfterHandlingBeforeUnload();
    }
    virtual void showContextMenu(const blink::WebContextMenuData& contextMenuData) {
        m_baseProxy->showContextMenu(Base::GetWebFrame()->toWebLocalFrame(), contextMenuData);
        Base::showContextMenu(contextMenuData);
    }
    virtual void didDetectXSS(blink::WebLocalFrame* frame, const blink::WebURL& insecureURL, bool didBlockEntirePage)
    {
        // This is not implemented in RenderFrameImpl, so need to explicitly call
        // into the base proxy.
        m_baseProxy->didDetectXSS(frame, insecureURL, didBlockEntirePage);
        Base::didDetectXSS(frame, insecureURL, didBlockEntirePage);
    }
    virtual void didDispatchPingLoader(blink::WebLocalFrame* frame, const blink::WebURL& url)
    {
        // This is not implemented in RenderFrameImpl, so need to explicitly call
        // into the base proxy.
        m_baseProxy->didDispatchPingLoader(frame, url);
        Base::didDispatchPingLoader(frame, url);
    }
    virtual void willRequestResource(blink::WebLocalFrame* frame, const blink::WebCachedURLRequest& request)
    {
        // This is not implemented in RenderFrameImpl, so need to explicitly call
        // into the base proxy.
        m_baseProxy->willRequestResource(frame, request);
        Base::willRequestResource(frame, request);
    }
    virtual void didCreateDataSource(blink::WebLocalFrame* frame, blink::WebDataSource* ds)
    {
        Base::didCreateDataSource(frame, ds);
    }
    virtual void willSendRequest(blink::WebLocalFrame* frame, unsigned identifier, blink::WebURLRequest& request, const blink::WebURLResponse& redirectResponse)
    {
        m_baseProxy->willSendRequest(frame, identifier, request, redirectResponse);
        Base::willSendRequest(frame, identifier, request, redirectResponse);
    }
    virtual void didReceiveResponse(blink::WebLocalFrame* frame, unsigned identifier, const blink::WebURLResponse& response)
    {
        m_baseProxy->didReceiveResponse(frame, identifier, response);
        Base::didReceiveResponse(frame, identifier, response);
    }
    virtual void didChangeResourcePriority(blink::WebLocalFrame* frame, unsigned identifier, const blink::WebURLRequest::Priority& priority, int intra_priority_value)
    {
        // This is not implemented in RenderFrameImpl, so need to explicitly call
        // into the base proxy.
        m_baseProxy->didChangeResourcePriority(frame, identifier, priority, intra_priority_value);
        Base::didChangeResourcePriority(frame, identifier, priority, intra_priority_value);
    }
    virtual void didFinishResourceLoad(blink::WebLocalFrame* frame, unsigned identifier)
    {
        Base::didFinishResourceLoad(frame, identifier);
    }
    virtual blink::WebNavigationPolicy decidePolicyForNavigation(blink::WebLocalFrame* frame, blink::WebDataSource::ExtraData* extraData, const blink::WebURLRequest& request, blink::WebNavigationType type, blink::WebNavigationPolicy defaultPolicy, bool isRedirect)
    {
        blink::WebNavigationPolicy policy = m_baseProxy->decidePolicyForNavigation(frame, extraData, request, type, defaultPolicy, isRedirect);
        if (policy == blink::WebNavigationPolicyIgnore)
            return policy;

        return Base::decidePolicyForNavigation(frame, extraData, request, type, defaultPolicy, isRedirect);
    }
    virtual bool willCheckAndDispatchMessageEvent(blink::WebLocalFrame* sourceFrame, blink::WebFrame* targetFrame, blink::WebSecurityOrigin target, blink::WebDOMMessageEvent event)
    {
        if (m_baseProxy->willCheckAndDispatchMessageEvent(sourceFrame, targetFrame, target, event))
            return true;
        return Base::willCheckAndDispatchMessageEvent(sourceFrame, targetFrame, target, event);
    }
    virtual void didStopLoading()
    {
        m_baseProxy->didStopLoading();
        Base::didStopLoading();
    }

private:
    WebTestProxyBase* m_baseProxy;

    // This is used to incrementally change code between Blink and Chromium.
    // It is used instead of a #define and is set by layouttest_support when
    // creating this object.
    int m_version;

    DISALLOW_COPY_AND_ASSIGN(WebFrameTestProxy);
};

}

#endif // WebTestProxy_h
