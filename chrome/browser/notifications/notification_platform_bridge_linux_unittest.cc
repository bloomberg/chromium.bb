// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_linux.h"

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/i18n/number_formatting.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

using message_center::Notification;
using testing::_;
using testing::ByMove;
using testing::Return;
using testing::StrictMock;

namespace {

const char kFreedesktopNotificationsName[] = "org.freedesktop.Notifications";
const char kFreedesktopNotificationsPath[] = "/org/freedesktop/Notifications";

class NotificationBuilder {
 public:
  explicit NotificationBuilder(const std::string& id)
      : notification_(message_center::NOTIFICATION_TYPE_SIMPLE,
                      id,
                      base::string16(),
                      base::string16(),
                      gfx::Image(),
                      base::string16(),
                      GURL(),
                      message_center::NotifierId(GURL()),
                      message_center::RichNotificationData(),
                      new message_center::NotificationDelegate()) {}

  Notification GetResult() { return notification_; }

  NotificationBuilder& SetImage(const gfx::Image& image) {
    notification_.set_image(image);
    return *this;
  }

  NotificationBuilder& SetItems(
      const std::vector<message_center::NotificationItem>& items) {
    notification_.set_items(items);
    return *this;
  }

  NotificationBuilder& SetMessage(const base::string16& message) {
    notification_.set_message(message);
    return *this;
  }

  NotificationBuilder& SetNeverTimeout(bool never_timeout) {
    notification_.set_never_timeout(never_timeout);
    return *this;
  }

  NotificationBuilder& SetOriginUrl(const GURL& origin_url) {
    notification_.set_origin_url(origin_url);
    return *this;
  }

  NotificationBuilder& SetProgress(int progress) {
    notification_.set_progress(progress);
    return *this;
  }

  NotificationBuilder& SetSilent(bool silent) {
    notification_.set_silent(silent);
    return *this;
  }

  NotificationBuilder& SetTitle(const base::string16& title) {
    notification_.set_title(title);
    return *this;
  }

  NotificationBuilder& SetType(message_center::NotificationType type) {
    notification_.set_type(type);
    return *this;
  }

 private:
  Notification notification_;
};

struct NotificationRequest {
  struct Action {
    std::string id;
    std::string label;
  };

  std::string summary;
  std::string body;
  std::vector<Action> actions;
  int32_t expire_timeout = 0;
  bool silent = false;
};

const SkBitmap CreateBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseARGB(255, 0, 255, 0);
  return bitmap;
}

gfx::ImageSkia CreateImageSkia(int width, int height) {
  return gfx::ImageSkia::CreateFrom1xBitmap(CreateBitmap(width, height));
}

NotificationRequest ParseRequest(dbus::MethodCall* method_call) {
  // The "Notify" message must have type (susssasa{sv}i).
  // https://developer.gnome.org/notification-spec/#command-notify
  NotificationRequest request;

  dbus::MessageReader reader(method_call);
  std::string str;
  uint32_t uint32;
  EXPECT_TRUE(reader.PopString(&str));              // app_name
  EXPECT_TRUE(reader.PopUint32(&uint32));           // replaces_id
  EXPECT_TRUE(reader.PopString(&str));              // app_icon
  EXPECT_TRUE(reader.PopString(&request.summary));  // summary
  EXPECT_TRUE(reader.PopString(&request.body));     // body

  {
    dbus::MessageReader actions_reader(nullptr);
    EXPECT_TRUE(reader.PopArray(&actions_reader));
    while (actions_reader.HasMoreData()) {
      // Actions come in pairs.
      std::string id;
      std::string label;
      EXPECT_TRUE(actions_reader.PopString(&id));
      EXPECT_TRUE(actions_reader.PopString(&label));
      request.actions.push_back({id, label});
    }
  }

  {
    dbus::MessageReader hints_reader(nullptr);
    EXPECT_TRUE(reader.PopArray(&hints_reader));
    while (hints_reader.HasMoreData()) {
      dbus::MessageReader dict_entry_reader(nullptr);
      EXPECT_TRUE(hints_reader.PopDictEntry(&dict_entry_reader));
      EXPECT_TRUE(dict_entry_reader.PopString(&str));
      dbus::MessageReader variant_reader(nullptr);
      if (str == "suppress-sound") {
        bool suppress_sound;
        EXPECT_TRUE(dict_entry_reader.PopVariantOfBool(&suppress_sound));
        request.silent = suppress_sound;
      } else {
        EXPECT_TRUE(dict_entry_reader.PopVariant(&variant_reader));
      }
      EXPECT_FALSE(dict_entry_reader.HasMoreData());
    }
  }

  EXPECT_TRUE(reader.PopInt32(&request.expire_timeout));
  EXPECT_FALSE(reader.HasMoreData());

  return request;
}

std::unique_ptr<dbus::Response> GetIdResponse(uint32_t id) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  writer.AppendUint32(id);
  return response;
}

ACTION_P(OnGetServerInformation, server_name) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  writer.AppendString(server_name);  // name
  writer.AppendString("chromium");   // vendor
  writer.AppendString("1.0");        // version
  writer.AppendString("1.2");        // spec_version
  return response;
}

ACTION_P(RegisterSignalCallback, callback_addr) {
  *callback_addr = arg2;
  std::move(*arg3).Run("" /* interface_name */, "" /* signal_name */,
                       true /* success */);
}

ACTION_P2(OnNotify, verifier, id) {
  verifier(ParseRequest(arg0));
  return GetIdResponse(id);
}

ACTION(OnCloseNotification) {
  // The "CloseNotification" message must have type (u).
  // https://developer.gnome.org/notification-spec/#command-close-notification
  dbus::MethodCall* method_call = arg0;
  dbus::MessageReader reader(method_call);
  uint32_t uint32;
  EXPECT_TRUE(reader.PopUint32(&uint32));
  EXPECT_FALSE(reader.HasMoreData());

  return dbus::Response::CreateEmpty();
}

ACTION_P(OnNotificationBridgeReady, success) {
  EXPECT_EQ(success, arg0);
}

// Matches a method call to the specified dbus target.
MATCHER_P(Calls, member, "") {
  return arg->GetMember() == member;
}

}  // namespace

class NotificationPlatformBridgeLinuxTest : public testing::Test {
 public:
  NotificationPlatformBridgeLinuxTest() = default;
  ~NotificationPlatformBridgeLinuxTest() override = default;

  void SetUp() override {
    mock_bus_ = new dbus::MockBus(dbus::Bus::Options());
    mock_notification_proxy_ = new StrictMock<dbus::MockObjectProxy>(
        mock_bus_.get(), kFreedesktopNotificationsName,
        dbus::ObjectPath(kFreedesktopNotificationsPath));
  }

  void TearDown() override {
    notification_bridge_linux_->CleanUp();
    content::RunAllTasksUntilIdle();
    notification_bridge_linux_.reset();
    mock_notification_proxy_ = nullptr;
    mock_bus_ = nullptr;
  }

 protected:
  void CreateNotificationBridgeLinux() {
    CreateNotificationBridgeLinux(
        std::vector<std::string>{"actions", "body", "body-hyperlinks",
                                 "body-images", "body-markup"},
        "NPBL_unittest", true, true, true);
  }

  // TODO(thomasanderson): Use a parameter builder instead of passing everything
  // directly.
  void CreateNotificationBridgeLinux(
      const std::vector<std::string>& capabilities,
      const std::string& server_name,
      bool expect_init_success,
      bool expect_shutdown,
      bool connect_signals) {
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(kFreedesktopNotificationsName,
                               dbus::ObjectPath(kFreedesktopNotificationsPath)))
        .WillOnce(Return(mock_notification_proxy_.get()));

    std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    writer.AppendArrayOfStrings(capabilities);
    EXPECT_CALL(*mock_notification_proxy_.get(),
                CallMethodAndBlock(Calls("GetCapabilities"), _))
        .WillOnce(Return(ByMove(std::move(response))));

    if (expect_init_success) {
      EXPECT_CALL(*mock_notification_proxy_.get(),
                  CallMethodAndBlock(Calls("GetServerInformation"), _))
          .WillOnce(OnGetServerInformation(server_name));
    }

    if (connect_signals) {
      EXPECT_CALL(*mock_notification_proxy_.get(),
                  DoConnectToSignal(kFreedesktopNotificationsName,
                                    "ActionInvoked", _, _))
          .WillOnce(RegisterSignalCallback(&action_invoked_callback_));

      EXPECT_CALL(*mock_notification_proxy_.get(),
                  DoConnectToSignal(kFreedesktopNotificationsName,
                                    "NotificationClosed", _, _))
          .WillOnce(RegisterSignalCallback(&notification_closed_callback_));
    }

    EXPECT_CALL(*this, MockableNotificationBridgeReadyCallback(_))
        .WillOnce(OnNotificationBridgeReady(expect_init_success));

    if (expect_shutdown)
      EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock());

    notification_bridge_linux_ =
        base::WrapUnique(new NotificationPlatformBridgeLinux(mock_bus_));
    notification_bridge_linux_->SetReadyCallback(
        base::BindOnce(&NotificationPlatformBridgeLinuxTest::
                           MockableNotificationBridgeReadyCallback,
                       base::Unretained(this)));
    content::RunAllTasksUntilIdle();
  }

  MOCK_METHOD1(MockableNotificationBridgeReadyCallback, void(bool));

  content::TestBrowserThreadBundle thread_bundle_;

  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_notification_proxy_;

  base::Callback<void(dbus::Signal*)> action_invoked_callback_;
  base::Callback<void(dbus::Signal*)> notification_closed_callback_;

  std::unique_ptr<NotificationPlatformBridgeLinux> notification_bridge_linux_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeLinuxTest);
};

TEST_F(NotificationPlatformBridgeLinuxTest, SetUpAndTearDown) {
  CreateNotificationBridgeLinux();
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotifyAndCloseFormat) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify([](const NotificationRequest&) {}, 1));
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("CloseNotification"), _))
      .WillOnce(OnCloseNotification());

  CreateNotificationBridgeLinux();
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, "", false,
      NotificationBuilder("").GetResult(), nullptr);
  notification_bridge_linux_->Close("", "");
}

TEST_F(NotificationPlatformBridgeLinuxTest, ProgressPercentageAddedToSummary) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [](const NotificationRequest& request) {
            EXPECT_EQ(
                base::UTF16ToUTF8(base::FormatPercent(42)) + " - The Title",
                request.summary);
          },
          1));

  CreateNotificationBridgeLinux();
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, "", false,
      NotificationBuilder("")
          .SetType(message_center::NOTIFICATION_TYPE_PROGRESS)
          .SetProgress(42)
          .SetTitle(base::UTF8ToUTF16("The Title"))
          .GetResult(),
      nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationListItemsInBody) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [](const NotificationRequest& request) {
            EXPECT_EQ("<b>abc</b> 123\n<b>def</b> 456", request.body);
          },
          1));

  CreateNotificationBridgeLinux();
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, "", false,
      NotificationBuilder("")
          .SetType(message_center::NOTIFICATION_TYPE_MULTIPLE)
          .SetItems(std::vector<message_center::NotificationItem>{
              {base::UTF8ToUTF16("abc"), base::UTF8ToUTF16("123")},
              {base::UTF8ToUTF16("def"), base::UTF8ToUTF16("456")}})
          .GetResult(),
      nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationTimeoutsNoPersistence) {
  const int32_t kExpireTimeout = 25000;
  const int32_t kExpireTimeoutNever = 0;
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ(kExpireTimeout, request.expire_timeout);
          },
          1))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ(kExpireTimeoutNever, request.expire_timeout);
          },
          2));

  CreateNotificationBridgeLinux();
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, "", false,
      NotificationBuilder("1").SetNeverTimeout(false).GetResult(), nullptr);
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, "", false,
      NotificationBuilder("2").SetNeverTimeout(true).GetResult(), nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest,
       NotificationTimeoutWithPersistence) {
  const int32_t kExpireTimeoutDefault = -1;
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ(kExpireTimeoutDefault, request.expire_timeout);
          },
          1));

  CreateNotificationBridgeLinux(
      std::vector<std::string>{"actions", "body", "persistence"},
      "NPBL_unittest", true, true, true);
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, "", false,
      NotificationBuilder("1").GetResult(), nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationImages) {
  const int kMaxImageWidth = 200;
  const int kMaxImageHeight = 100;

  const int original_width = kMaxImageWidth * 2;
  const int original_height = kMaxImageHeight;
  const int expected_width = kMaxImageWidth;
  const int expected_height = kMaxImageHeight / 2;

  gfx::Image original_image =
      gfx::Image(CreateImageSkia(original_width, original_height));

  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            std::string file_name;
            EXPECT_TRUE(RE2::FullMatch(
                request.body, "\\<img src=\\\"(.+)\\\" alt=\\\".*\\\"/\\>",
                &file_name));
            std::string file_contents;
            EXPECT_TRUE(base::ReadFileToString(base::FilePath(file_name),
                                               &file_contents));
            gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(
                reinterpret_cast<const unsigned char*>(file_contents.c_str()),
                file_contents.size());
            EXPECT_EQ(expected_width, image.Width());
            EXPECT_EQ(expected_height, image.Height());
          },
          1));

  CreateNotificationBridgeLinux();
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, "", false,
      NotificationBuilder("")
          .SetType(message_center::NOTIFICATION_TYPE_IMAGE)
          .SetImage(original_image)
          .GetResult(),
      nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationAttribution) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [](const NotificationRequest& request) {
            EXPECT_EQ(
                "<a href=\"https://google.com/"
                "search?q=test&amp;ie=UTF8\">google.com</a>\n\nBody text",
                request.body);
          },
          1));

  CreateNotificationBridgeLinux();
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, "", false,
      NotificationBuilder("")
          .SetMessage(base::ASCIIToUTF16("Body text"))
          .SetOriginUrl(GURL("https://google.com/search?q=test&ie=UTF8"))
          .GetResult(),
      nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest, MissingActionsCapability) {
  CreateNotificationBridgeLinux(std::vector<std::string>{"body"},
                                "NPBL_unittest", false, true, false);
}

TEST_F(NotificationPlatformBridgeLinuxTest, MissingBodyCapability) {
  CreateNotificationBridgeLinux(std::vector<std::string>{"actions"},
                                "NPBL_unittest", false, true, false);
}

TEST_F(NotificationPlatformBridgeLinuxTest, EscapeHtml) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [](const NotificationRequest& request) {
            EXPECT_EQ("&lt;span id='1' class=\"2\"&gt;&amp;#39;&lt;/span&gt;",
                      request.body);
          },
          1));

  CreateNotificationBridgeLinux();
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, "", false,
      NotificationBuilder("")
          .SetMessage(
              base::ASCIIToUTF16("<span id='1' class=\"2\">&#39;</span>"))
          .GetResult(),
      nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest, Silent) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_FALSE(request.silent);
          },
          1))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_TRUE(request.silent);
          },
          2));

  CreateNotificationBridgeLinux();
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, "", false,
      NotificationBuilder("1").SetSilent(false).GetResult(), nullptr);
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, "", false,
      NotificationBuilder("2").SetSilent(true).GetResult(), nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest, OriginUrlFormat) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ("google.com", request.body);
          },
          1))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ("mail.google.com", request.body);
          },
          2))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ("123.123.123.123", request.body);
          },
          3))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ("a.b.c.co.uk", request.body);
          },
          4))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ("evilsite.com", request.body);
          },
          4));

  CreateNotificationBridgeLinux(std::vector<std::string>{"actions", "body"},
                                "NPBL_unittest", true, true, true);
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, "", false,
      NotificationBuilder("1")
          .SetOriginUrl(GURL("https://google.com"))
          .GetResult(),
      nullptr);
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, "", false,
      NotificationBuilder("2")
          .SetOriginUrl(GURL("https://mail.google.com"))
          .GetResult(),
      nullptr);
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, "", false,
      NotificationBuilder("3")
          .SetOriginUrl(GURL("https://123.123.123.123"))
          .GetResult(),
      nullptr);
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, "", false,
      NotificationBuilder("4")
          .SetOriginUrl(GURL("https://a.b.c.co.uk/file.html"))
          .GetResult(),
      nullptr);
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, "", false,
      NotificationBuilder("5")
          .SetOriginUrl(GURL(
              "https://google.com.blahblahblahblahblahblahblah.evilsite.com"))
          .GetResult(),
      nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest,
       CinnamonNotificationsHaveClosebutton) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [](const NotificationRequest& request) {
            EXPECT_EQ(3UL, request.actions.size());
            EXPECT_EQ("default", request.actions[0].id);
            EXPECT_EQ("Activate", request.actions[0].label);
            EXPECT_EQ("settings", request.actions[1].id);
            EXPECT_EQ("Settings", request.actions[1].label);
            EXPECT_EQ("close", request.actions[2].id);
            EXPECT_EQ("Close", request.actions[2].label);
          },
          1));

  CreateNotificationBridgeLinux(std::vector<std::string>{"actions", "body"},
                                "cinnamon", true, true, true);
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, "", false,
      NotificationBuilder("").GetResult(), nullptr);
}
