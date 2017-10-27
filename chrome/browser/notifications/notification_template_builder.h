// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEMPLATE_BUILDER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEMPLATE_BUILDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"

class GURL;
class XmlWriter;

namespace message_center {
struct ButtonInfo;
class Notification;
}

// Builds XML-based notification templates for displaying a given notification
// in the Windows Action Center.
//
// https://docs.microsoft.com/en-us/windows/uwp/controls-and-patterns/tiles-and-notifications-adaptive-interactive-toasts
//
// libXml was preferred (over WinXml, which the samples tend to use) because it
// is used frequently in Chrome, is nicer to use and has already been vetted.
class NotificationTemplateBuilder {
 public:
  // Builds the notification template for the given |notification|. The given
  // |notification_id| will be available when the user interacts with the toast.
  static std::unique_ptr<NotificationTemplateBuilder> Build(
      const std::string& notification_id,
      const message_center::Notification& notification);

  ~NotificationTemplateBuilder();

  // Gets the XML template that was created by this builder.
  base::string16 GetNotificationTemplate() const;

 private:
  // The different types of text nodes to output.
  enum class TextType { NORMAL, ATTRIBUTION };

  NotificationTemplateBuilder();

  // Formats the |origin| for display in the notification template.
  std::string FormatOrigin(const GURL& origin) const;

  // Writes the <toast> element with the |notification_id| as the launch string.
  // Also closes the |xml_writer_| for writing as the toast is now complete.
  void StartToastElement(const std::string& notification_id);
  void EndToastElement();

  // Writes the <visual> element.
  void StartVisualElement();
  void EndVisualElement();

  // Writes the <binding> element with the given |template_name|.
  void StartBindingElement(const std::string& template_name);
  void EndBindingElement();

  // Writes the <text> element with the given |id| and |content|. If
  // |text_type| is ATTRIBUTION then |content| is treated as the source that the
  // notification is attributed to.
  void WriteTextElement(const std::string& id,
                        const std::string& content,
                        TextType text_type);

  // Writes the <actions> element.
  void StartActionsElement();
  void EndActionsElement();

  // Writes the <audio silent="true"> element.
  void WriteAudioSilentElement();

  // Fills in the details for the actions.
  void AddActions(const std::vector<message_center::ButtonInfo>& buttons);
  void WriteActionElement(const message_center::ButtonInfo& button, int index);

  // The XML writer to which the template will be written.
  std::unique_ptr<XmlWriter> xml_writer_;

  DISALLOW_COPY_AND_ASSIGN(NotificationTemplateBuilder);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEMPLATE_BUILDER_H_
