// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/WebPermissions.h"

#include "content/shell/renderer/test_runner/TestCommon.h"
#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebURL.h"

using namespace std;

namespace WebTestRunner {

WebPermissions::WebPermissions()
    : m_delegate(0)
{
    reset();
}

WebPermissions::~WebPermissions()
{
}

bool WebPermissions::allowImage(bool enabledPerSettings, const blink::WebURL& imageURL)
{
    bool allowed = enabledPerSettings && m_imagesAllowed;
    if (m_dumpCallbacks && m_delegate)
        m_delegate->printMessage(std::string("PERMISSION CLIENT: allowImage(") + normalizeLayoutTestURL(imageURL.spec()) + "): " + (allowed ? "true" : "false") + "\n");
    return allowed;
}

bool WebPermissions::allowScriptFromSource(bool enabledPerSettings, const blink::WebURL& scriptURL)
{
    bool allowed = enabledPerSettings && m_scriptsAllowed;
    if (m_dumpCallbacks && m_delegate)
        m_delegate->printMessage(std::string("PERMISSION CLIENT: allowScriptFromSource(") + normalizeLayoutTestURL(scriptURL.spec()) + "): " + (allowed ? "true" : "false") + "\n");
    return allowed;
}

bool WebPermissions::allowStorage(bool)
{
    return m_storageAllowed;
}

bool WebPermissions::allowPlugins(bool enabledPerSettings)
{
    return enabledPerSettings && m_pluginsAllowed;
}

bool WebPermissions::allowDisplayingInsecureContent(bool enabledPerSettings, const blink::WebSecurityOrigin&, const blink::WebURL&)
{
    return enabledPerSettings || m_displayingInsecureContentAllowed;
}

bool WebPermissions::allowRunningInsecureContent(bool enabledPerSettings, const blink::WebSecurityOrigin&, const blink::WebURL&)
{
    return enabledPerSettings || m_runningInsecureContentAllowed;
}

void WebPermissions::setImagesAllowed(bool imagesAllowed)
{
    m_imagesAllowed = imagesAllowed;
}

void WebPermissions::setScriptsAllowed(bool scriptsAllowed)
{
    m_scriptsAllowed = scriptsAllowed;
}

void WebPermissions::setStorageAllowed(bool storageAllowed)
{
    m_storageAllowed = storageAllowed;
}

void WebPermissions::setPluginsAllowed(bool pluginsAllowed)
{
    m_pluginsAllowed = pluginsAllowed;
}

void WebPermissions::setDisplayingInsecureContentAllowed(bool allowed)
{
    m_displayingInsecureContentAllowed = allowed;
}

void WebPermissions::setRunningInsecureContentAllowed(bool allowed)
{
    m_runningInsecureContentAllowed = allowed;
}

void WebPermissions::setDelegate(WebTestDelegate* delegate)
{
    m_delegate = delegate;
}

void WebPermissions::setDumpCallbacks(bool dumpCallbacks)
{
    m_dumpCallbacks = dumpCallbacks;
}

void WebPermissions::reset()
{
    m_dumpCallbacks = false;
    m_imagesAllowed = true;
    m_scriptsAllowed = true;
    m_storageAllowed = true;
    m_pluginsAllowed = true;
    m_displayingInsecureContentAllowed = false;
    m_runningInsecureContentAllowed = false;
}

}
