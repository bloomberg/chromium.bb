// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/NotificationPresenter.h"

#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebNotificationPermissionCallback.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "url/gurl.h"

using namespace blink;
using namespace std;

namespace WebTestRunner {

namespace {

WebString identifierForNotification(const WebNotification& notification)
{
    if (notification.isHTML())
        return notification.url().spec().utf16();
    return notification.title();
}

void deferredDisplayDispatch(void* context)
{
    WebNotification* notification = static_cast<WebNotification*>(context);
    notification->dispatchDisplayEvent();
    delete notification;
}

}

NotificationPresenter::NotificationPresenter()
    : m_delegate(0)
{
}

NotificationPresenter::~NotificationPresenter()
{
}

void NotificationPresenter::grantPermission(const WebString& origin)
{
    m_allowedOrigins.insert(origin.utf8());
}

bool NotificationPresenter::simulateClick(const WebString& title)
{
    string id(title.utf8());
    if (m_activeNotifications.find(id) == m_activeNotifications.end())
        return false;

    const WebNotification& notification = m_activeNotifications.find(id)->second;
    WebNotification eventTarget(notification);
    eventTarget.dispatchClickEvent();
    return true;
}

void NotificationPresenter::cancelAllActiveNotifications()
{
    while (!m_activeNotifications.empty()) {
        const WebNotification& notification = m_activeNotifications.begin()->second;
        cancel(notification);
    }
}

// The output from all these methods matches what DumpRenderTree produces.
bool NotificationPresenter::show(const WebNotification& notification)
{
    WebString identifier = identifierForNotification(notification);
    if (!notification.replaceId().isEmpty()) {
        string replaceId(notification.replaceId().utf8());
        if (m_replacements.find(replaceId) != m_replacements.end())
            m_delegate->printMessage(string("REPLACING NOTIFICATION ") + m_replacements.find(replaceId)->second + "\n");

        m_replacements[replaceId] = identifier.utf8();
    }

    if (notification.isHTML())
        m_delegate->printMessage(string("DESKTOP NOTIFICATION: contents at ") + string(notification.url().spec()) + "\n");
    else {
        m_delegate->printMessage("DESKTOP NOTIFICATION:");
        m_delegate->printMessage(notification.direction() == WebTextDirectionRightToLeft ? "(RTL)" : "");
        m_delegate->printMessage(" icon ");
        m_delegate->printMessage(notification.iconURL().isEmpty() ? "" : notification.iconURL().spec().data());
        m_delegate->printMessage(", title ");
        m_delegate->printMessage(notification.title().isEmpty() ? "" : notification.title().utf8().data());
        m_delegate->printMessage(", text ");
        m_delegate->printMessage(notification.body().isEmpty() ? "" : notification.body().utf8().data());
        m_delegate->printMessage("\n");
    }

    string id(identifier.utf8());
    m_activeNotifications[id] = notification;

    Platform::current()->callOnMainThread(deferredDisplayDispatch, new WebNotification(notification));
    return true;
}

void NotificationPresenter::cancel(const WebNotification& notification)
{
    WebString identifier = identifierForNotification(notification);
    m_delegate->printMessage(string("DESKTOP NOTIFICATION CLOSED: ") + string(identifier.utf8()) + "\n");
    WebNotification eventTarget(notification);
    eventTarget.dispatchCloseEvent(false);

    string id(identifier.utf8());
    m_activeNotifications.erase(id);
}

void NotificationPresenter::objectDestroyed(const blink::WebNotification& notification)
{
    WebString identifier = identifierForNotification(notification);
    string id(identifier.utf8());
    m_activeNotifications.erase(id);
}

WebNotificationPresenter::Permission NotificationPresenter::checkPermission(const WebSecurityOrigin& origin)
{
    // Check with the layout test controller
    WebString originString = origin.toString();
    bool allowed = m_allowedOrigins.find(string(originString.utf8())) != m_allowedOrigins.end();
    return allowed ? WebNotificationPresenter::PermissionAllowed
        : WebNotificationPresenter::PermissionDenied;
}

void NotificationPresenter::requestPermission(
    const WebSecurityOrigin& origin,
    WebNotificationPermissionCallback* callback)
{
    m_delegate->printMessage("DESKTOP NOTIFICATION PERMISSION REQUESTED: " + string(origin.toString().utf8()) + "\n");
    callback->permissionRequestComplete();
}

}
