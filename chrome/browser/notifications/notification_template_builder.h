// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEMPLATE_BUILDER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEMPLATE_BUILDER_H_

#include <memory>
#include <string>

#include "base/macros.h"

class GURL;
class XmlWriter;

namespace message_center {
class Notification;
}

// Builds XML-based notification templates for displaying a given notification
// in the Windows Action Center.
//
// https://docs.microsoft.com/en-us/uwp/schemas/tiles/toastschema/schema-root
// https://msdn.microsoft.com/library/1a437614-4259-426b-8e3f-ca57368b2e7a
//
// The current builder is a best-effort implementation that supports the title
// body text and attribution of a notification.
class NotificationTemplateBuilder {
 public:
  // Builds the notification template for the given |notification|. The given
  // |notification_id| will be available when the user interacts with the toast.
  static std::unique_ptr<NotificationTemplateBuilder> Build(
      const std::string& notification_id,
      const message_center::Notification& notification);

  ~NotificationTemplateBuilder();

  // Gets the XML template that was created by this builder.
  std::string GetNotificationTemplate();

 private:
  NotificationTemplateBuilder();

  // Formats the |origin| for display in the notification template.
  std::string FormatOrigin(const GURL& origin) const;

  // Writes the <toast> element with the |notification_id| as the launch string.
  // Also closes the |xml_writer_| for writing as the toast is now complete.
  // https://docs.microsoft.com/en-us/uwp/schemas/tiles/toastschema/element-toast
  void StartToastElement(const std::string& notification_id);
  void EndToastElement();

  // Writes the <visual> element.
  // https://docs.microsoft.com/en-us/uwp/schemas/tiles/toastschema/element-visual
  void StartVisualElement();
  void EndVisualElement();

  // Writes the <binding> element with the given |template_name|.
  // https://docs.microsoft.com/en-us/uwp/schemas/tiles/toastschema/element-binding
  void StartBindingElement(const std::string& template_name);
  void EndBindingElement();

  // Writes the <text> element with the given |id| and |content|.
  // https://docs.microsoft.com/en-us/uwp/schemas/tiles/toastschema/element-text
  void WriteTextElement(const std::string& id, const std::string& content);

  // The XML writer to which the template will be written.
  std::unique_ptr<XmlWriter> xml_writer_;

  DISALLOW_COPY_AND_ASSIGN(NotificationTemplateBuilder);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEMPLATE_BUILDER_H_
