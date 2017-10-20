// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_template_builder.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/elide_url.h"
#include "third_party/libxml/chromium/libxml_utils.h"
#include "ui/message_center/notification.h"
#include "url/origin.h"

namespace {

// Constants used for the XML element names and their attributes.
const char kActionElement[] = "action";
const char kActionsElement[] = "actions";
const char kActivationType[] = "activationType";
const char kArguments[] = "arguments";
const char kBindingElement[] = "binding";
const char kBindingElementTemplateAttribute[] = "template";
const char kButtonIndex[] = "buttonIndex=";
const char kContent[] = "content";
const char kForeground[] = "foreground";
const char kInputElement[] = "input";
const char kInputId[] = "id";
const char kInputType[] = "type";
const char kPlaceholderContent[] = "placeHolderContent";
const char kText[] = "text";
const char kUserResponse[] = "userResponse";
const char kTextElement[] = "text";
const char kToastElement[] = "toast";
const char kToastElementLaunchAttribute[] = "launch";
const char kVisualElement[] = "visual";

// Name of the template used for default Chrome notifications.
const char kDefaultTemplate[] = "ToastGeneric";

// The XML version header that has to be stripped from the output.
const char kXmlVersionHeader[] = "<?xml version=\"1.0\"?>\n";

}  // namespace

// static
std::unique_ptr<NotificationTemplateBuilder> NotificationTemplateBuilder::Build(
    const std::string& notification_id,
    const message_center::Notification& notification) {
  std::unique_ptr<NotificationTemplateBuilder> builder =
      base::WrapUnique(new NotificationTemplateBuilder);

  // TODO(finnur): Can we set <toast scenario="reminder"> for notifications
  // that have set the never_timeout() flag?
  builder->StartToastElement(notification_id);
  builder->StartVisualElement();

  // TODO(finnur): Set the correct binding template based on the |notification|.
  builder->StartBindingElement(kDefaultTemplate);

  // Content for the toast template.
  builder->WriteTextElement("1", base::UTF16ToUTF8(notification.title()));
  builder->WriteTextElement("2", base::UTF16ToUTF8(notification.message()));
  builder->WriteTextElement("3",
                            builder->FormatOrigin(notification.origin_url()));

  builder->EndBindingElement();
  builder->EndVisualElement();

  builder->AddActions(notification.buttons());

  builder->EndToastElement();

  return builder;
}

NotificationTemplateBuilder::NotificationTemplateBuilder()
    : xml_writer_(std::make_unique<XmlWriter>()) {
  xml_writer_->StartWriting();
}

NotificationTemplateBuilder::~NotificationTemplateBuilder() = default;

base::string16 NotificationTemplateBuilder::GetNotificationTemplate() const {
  std::string template_xml = xml_writer_->GetWrittenString();
  DCHECK(base::StartsWith(template_xml, kXmlVersionHeader,
                          base::CompareCase::SENSITIVE));

  // The |kXmlVersionHeader| is automatically appended by libxml, but the toast
  // system in the Windows Action Center expects it to be absent.
  return base::UTF8ToUTF16(template_xml.substr(sizeof(kXmlVersionHeader) - 1));
}

std::string NotificationTemplateBuilder::FormatOrigin(
    const GURL& origin) const {
  base::string16 origin_string = url_formatter::FormatOriginForSecurityDisplay(
      url::Origin::Create(origin),
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  DCHECK(origin_string.size());

  return base::UTF16ToUTF8(origin_string);
}

void NotificationTemplateBuilder::StartToastElement(
    const std::string& notification_id) {
  xml_writer_->StartElement(kToastElement);
  xml_writer_->AddAttribute(kToastElementLaunchAttribute, notification_id);
}

void NotificationTemplateBuilder::EndToastElement() {
  xml_writer_->EndElement();
  xml_writer_->StopWriting();
}

void NotificationTemplateBuilder::StartVisualElement() {
  xml_writer_->StartElement(kVisualElement);
}

void NotificationTemplateBuilder::EndVisualElement() {
  xml_writer_->EndElement();
}

void NotificationTemplateBuilder::StartBindingElement(
    const std::string& template_name) {
  xml_writer_->StartElement(kBindingElement);
  xml_writer_->AddAttribute(kBindingElementTemplateAttribute, template_name);
}

void NotificationTemplateBuilder::EndBindingElement() {
  xml_writer_->EndElement();
}

void NotificationTemplateBuilder::WriteTextElement(const std::string& id,
                                                   const std::string& content) {
  xml_writer_->StartElement(kTextElement);
  xml_writer_->AppendElementContent(content);
  xml_writer_->EndElement();
}

void NotificationTemplateBuilder::AddActions(
    const std::vector<message_center::ButtonInfo>& buttons) {
  if (!buttons.size())
    return;

  StartActionsElement();

  bool inline_reply = false;
  std::string placeholder;
  for (const auto& button : buttons) {
    if (button.type != message_center::ButtonType::TEXT)
      continue;

    inline_reply = true;
    placeholder = base::UTF16ToUTF8(button.placeholder);
    break;
  }

  if (inline_reply) {
    xml_writer_->StartElement(kInputElement);
    xml_writer_->AddAttribute(kInputId, kUserResponse);
    xml_writer_->AddAttribute(kInputType, kText);
    xml_writer_->AddAttribute(kPlaceholderContent, placeholder);
    xml_writer_->EndElement();
  }

  for (size_t i = 0; i < buttons.size(); ++i) {
    const auto& button = buttons[i];
    WriteActionElement(button, i);
  }

  EndActionsElement();
}

void NotificationTemplateBuilder::StartActionsElement() {
  xml_writer_->StartElement(kActionsElement);
}

void NotificationTemplateBuilder::EndActionsElement() {
  xml_writer_->EndElement();
}

void NotificationTemplateBuilder::WriteActionElement(
    const message_center::ButtonInfo& button,
    int index) {
  // TODO(finnur): Implement button images (imageUri).

  xml_writer_->StartElement(kActionElement);
  xml_writer_->AddAttribute(kActivationType, kForeground);
  xml_writer_->AddAttribute(kContent, base::UTF16ToUTF8(button.title).c_str());
  std::string param = std::string(kButtonIndex) + base::IntToString(index);
  xml_writer_->AddAttribute(kArguments, param.c_str());
  xml_writer_->EndElement();
}
