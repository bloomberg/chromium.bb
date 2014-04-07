// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBPERMISSIONS_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBPERMISSIONS_H_

#include "base/basictypes.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebPermissionClient.h"

namespace WebTestRunner {

class WebTestDelegate;

class WebPermissions : public blink::WebPermissionClient {
public:
    WebPermissions();
    virtual ~WebPermissions();

    // Override WebPermissionClient methods.
    // TODO(jam): remove all these methods after Blink rolls.
    virtual bool allowImage(blink::WebLocalFrame*, bool enabledPerSettings, const blink::WebURL& imageURL);
    virtual bool allowScriptFromSource(blink::WebLocalFrame*, bool enabledPerSettings, const blink::WebURL& scriptURL);
    virtual bool allowStorage(blink::WebLocalFrame*, bool local);
    virtual bool allowPlugins(blink::WebLocalFrame*, bool enabledPerSettings);
    virtual bool allowDisplayingInsecureContent(blink::WebLocalFrame*, bool enabledPerSettings, const blink::WebSecurityOrigin&, const blink::WebURL&);
    virtual bool allowRunningInsecureContent(blink::WebLocalFrame*, bool enabledPerSettings, const blink::WebSecurityOrigin&, const blink::WebURL&);

    virtual bool allowImage(bool enabledPerSettings, const blink::WebURL& imageURL);
    virtual bool allowScriptFromSource(bool enabledPerSettings, const blink::WebURL& scriptURL);
    virtual bool allowStorage(bool local);
    virtual bool allowPlugins(bool enabledPerSettings);
    virtual bool allowDisplayingInsecureContent(bool enabledPerSettings, const blink::WebSecurityOrigin&, const blink::WebURL&);
    virtual bool allowRunningInsecureContent(bool enabledPerSettings, const blink::WebSecurityOrigin&, const blink::WebURL&);

    // Hooks to set the different policies.
    void setImagesAllowed(bool);
    void setScriptsAllowed(bool);
    void setStorageAllowed(bool);
    void setPluginsAllowed(bool);
    void setDisplayingInsecureContentAllowed(bool);
    void setRunningInsecureContentAllowed(bool);

    // Resets the policy to allow everything, except for running insecure content.
    void reset();

    void setDelegate(WebTestDelegate*);
    void setDumpCallbacks(bool);

private:
    WebTestDelegate* m_delegate;
    bool m_dumpCallbacks;

    bool m_imagesAllowed;
    bool m_scriptsAllowed;
    bool m_storageAllowed;
    bool m_pluginsAllowed;
    bool m_displayingInsecureContentAllowed;
    bool m_runningInsecureContentAllowed;

    DISALLOW_COPY_AND_ASSIGN(WebPermissions);
};

}

#endif
