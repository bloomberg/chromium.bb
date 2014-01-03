// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_NOTIFICATIONPRESENTER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_NOTIFICATIONPRESENTER_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "third_party/WebKit/public/web/WebNotification.h"
#include "third_party/WebKit/public/web/WebNotificationPresenter.h"

namespace WebTestRunner {

class WebTestDelegate;

// A class that implements WebNotificationPresenter for the TestRunner library.
class NotificationPresenter : public blink::WebNotificationPresenter {
public:
    NotificationPresenter();
    virtual ~NotificationPresenter();

    void setDelegate(WebTestDelegate* delegate) { m_delegate = delegate; }

    // Called by the TestRunner to simulate a user granting permission.
    void grantPermission(const blink::WebString& origin);

    // Called by the TestRunner to simulate a user clicking on a notification.
    bool simulateClick(const blink::WebString& notificationIdentifier);

    // Called by the TestRunner to cancel all active notications.
    void cancelAllActiveNotifications();

    // blink::WebNotificationPresenter interface
    virtual bool show(const blink::WebNotification&);
    virtual void cancel(const blink::WebNotification&);
    virtual void objectDestroyed(const blink::WebNotification&);
    virtual Permission checkPermission(const blink::WebSecurityOrigin&);
    virtual void requestPermission(const blink::WebSecurityOrigin&, blink::WebNotificationPermissionCallback*);

    void reset() { m_allowedOrigins.clear(); }

private:
    WebTestDelegate* m_delegate;

    // Set of allowed origins.
    std::set<std::string> m_allowedOrigins;

    // Map of active notifications.
    std::map<std::string, blink::WebNotification> m_activeNotifications;

    // Map of active replacement IDs to the titles of those notifications
    std::map<std::string, std::string> m_replacements;

    DISALLOW_COPY_AND_ASSIGN(NotificationPresenter);
};

}

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_NOTIFICATIONPRESENTER_H_
