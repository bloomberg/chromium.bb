// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_linux.h"

#include <algorithm>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/notifications/native_notification_display_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration_linux.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "ui/gfx/image/image_skia.h"

namespace {

const char kFreedesktopNotificationsName[] = "org.freedesktop.Notifications";
const char kFreedesktopNotificationsPath[] = "/org/freedesktop/Notifications";

const char kDefaultButtonId[] = "default";
const char kSettingsButtonId[] = "settings";

gfx::Image DeepCopyImage(const gfx::Image& image) {
  std::unique_ptr<gfx::ImageSkia> image_skia(image.CopyImageSkia());
  return gfx::Image(*image_skia);
}

int NotificationPriorityToFdoUrgency(int priority) {
  enum FdoUrgency {
    LOW = 0,
    NORMAL = 1,
    CRITICAL = 2,
  };
  switch (priority) {
    case message_center::MIN_PRIORITY:
    case message_center::LOW_PRIORITY:
      return LOW;
    case message_center::HIGH_PRIORITY:
    case message_center::MAX_PRIORITY:
      return CRITICAL;
    default:
      NOTREACHED();
    case message_center::DEFAULT_PRIORITY:
      return NORMAL;
  }
}

// Runs once the profile has been loaded in order to perform a given
// |operation| on a notification.
void ProfileLoadedCallback(NotificationCommon::Operation operation,
                           NotificationCommon::Type notification_type,
                           const std::string& origin,
                           const std::string& notification_id,
                           int action_index,
                           const base::NullableString16& reply,
                           Profile* profile) {
  if (!profile)
    return;

  NotificationDisplayService* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile);

  static_cast<NativeNotificationDisplayService*>(display_service)
      ->ProcessNotificationOperation(operation, notification_type, origin,
                                     notification_id, action_index, reply);
}

void ForwardNotificationOperationOnUiThread(
    NotificationCommon::Operation operation,
    NotificationCommon::Type notification_type,
    const std::string& origin,
    const std::string& notification_id,
    int action_index,
    const std::string& profile_id,
    bool is_incognito) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  profile_manager->LoadProfile(
      profile_id, is_incognito,
      base::Bind(&ProfileLoadedCallback, operation, notification_type, origin,
                 notification_id, action_index, base::NullableString16()));
}

// Writes |data| to a new temporary file and returns its path.
// Returns base::FilePath() on failure.
base::FilePath WriteDataToTmpFile(
    const scoped_refptr<base::RefCountedMemory>& data) {
  int data_len = data->size();
  if (data_len == 0)
    return base::FilePath();
  base::FilePath file_path;
  if (!base::CreateTemporaryFile(&file_path))
    return base::FilePath();
  if (base::WriteFile(file_path, data->front_as<char>(), data_len) !=
      data_len) {
    base::DeleteFile(file_path, false);
    return base::FilePath();
  }
  return file_path;
}

}  // namespace

// static
NotificationPlatformBridge* NotificationPlatformBridge::Create() {
  return new NotificationPlatformBridgeLinux();
}

class NotificationPlatformBridgeLinuxImpl
    : public NotificationPlatformBridge,
      public content::NotificationObserver,
      public base::RefCountedThreadSafe<NotificationPlatformBridgeLinuxImpl> {
 public:
  explicit NotificationPlatformBridgeLinuxImpl(scoped_refptr<dbus::Bus> bus)
      : bus_(bus) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    task_runner_ = base::CreateSingleThreadTaskRunnerWithTraits(
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
    registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                   content::NotificationService::AllSources());
  }

  // InitOnTaskRunner() cannot be posted from within the constructor
  // because of a race condition.  The reference count for |this|
  // starts out as 0.  Posting the Init task would increment the count
  // to 1.  If the task finishes before the constructor returns, the
  // count will go to 0 and the object would be prematurely
  // destructed.
  void Init() {
    PostTaskToTaskRunnerThread(base::BindOnce(
        &NotificationPlatformBridgeLinuxImpl::InitOnTaskRunner, this));
  }

  void Display(NotificationCommon::Type notification_type,
               const std::string& notification_id,
               const std::string& profile_id,
               bool is_incognito,
               const Notification& notification) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // Notifications contain gfx::Image's which have reference counts
    // that are not thread safe.  Because of this, we duplicate the
    // notification and its images.  Wrap the notification in a
    // unique_ptr to transfer ownership of the notification (and the
    // non-thread-safe reference counts) to the task runner thread.
    auto notification_copy = base::MakeUnique<Notification>(notification);
    notification_copy->set_icon(DeepCopyImage(notification_copy->icon()));
    notification_copy->set_image(gfx::Image());
    notification_copy->set_small_image(gfx::Image());
    for (size_t i = 0; i < notification_copy->buttons().size(); i++)
      notification_copy->SetButtonIcon(i, gfx::Image());

    PostTaskToTaskRunnerThread(base::BindOnce(
        &NotificationPlatformBridgeLinuxImpl::DisplayOnTaskRunner, this,
        notification_type, notification_id, profile_id, is_incognito,
        base::Passed(&notification_copy)));
  }

  void Close(const std::string& profile_id,
             const std::string& notification_id) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    PostTaskToTaskRunnerThread(
        base::BindOnce(&NotificationPlatformBridgeLinuxImpl::CloseOnTaskRunner,
                       this, profile_id, notification_id));
  }

  void GetDisplayed(
      const std::string& profile_id,
      bool incognito,
      const GetDisplayedNotificationsCallback& callback) const override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    PostTaskToTaskRunnerThread(base::BindOnce(
        &NotificationPlatformBridgeLinuxImpl::GetDisplayedOnTaskRunner, this,
        profile_id, incognito, callback));
  }

  void SetReadyCallback(NotificationBridgeReadyCallback callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (connected_.has_value()) {
      std::move(callback).Run(connected_.value());
    } else {
      on_connected_callbacks_.push_back(std::move(callback));
    }
  }

  void CleanUp() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    PostTaskToTaskRunnerThread(base::BindOnce(
        &NotificationPlatformBridgeLinuxImpl::CleanUpOnTaskRunner, this));
  }

 private:
  friend class base::RefCountedThreadSafe<NotificationPlatformBridgeLinuxImpl>;

  struct ResourceFile {
    explicit ResourceFile(const base::FilePath& file_path)
        : file_path(file_path) {}
    ~ResourceFile() { base::DeleteFile(file_path, false); }
    const base::FilePath file_path;
  };

  struct NotificationData {
    NotificationData(NotificationCommon::Type notification_type,
                     const std::string& notification_id,
                     const std::string& profile_id,
                     bool is_incognito,
                     const GURL& origin_url)
        : notification_type(notification_type),
          notification_id(notification_id),
          profile_id(profile_id),
          is_incognito(is_incognito),
          origin_url(origin_url) {}

    // The ID used by the notification server.  Will be 0 until the
    // first "Notify" message completes.
    uint32_t dbus_id = 0;

    // Same parameters used by NotificationPlatformBridge::Display().
    NotificationCommon::Type notification_type;
    const std::string notification_id;
    const std::string profile_id;
    const bool is_incognito;

    // A copy of the origin_url from the underlying
    // message_center::Notification.  Used to pass back to
    // NativeNotificationDisplayService.
    const GURL origin_url;

    // Used to keep track of the IDs of the buttons currently displayed
    // on this notification.  The valid range of action IDs is
    // [action_start, action_end).
    size_t action_start = 0;
    size_t action_end = 0;

    // Temporary resource files associated with the notification that
    // should be cleaned up when the notification is closed or on
    // shutdown.
    std::vector<std::unique_ptr<ResourceFile>> resource_files;
  };

  ~NotificationPlatformBridgeLinuxImpl() override {
    DCHECK(!bus_);
    DCHECK(!notification_proxy_);
    DCHECK(notifications_.empty());
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
    // The browser process is about to exit.  Post the CleanUp() task
    // while we still can.
    CleanUp();
  }

  void PostTaskToUiThread(base::OnceClosure closure) const {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    bool success = content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE, std::move(closure));
    DCHECK(success);
  }

  void PostTaskToTaskRunnerThread(base::OnceClosure closure) const {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(task_runner_);
    bool success = task_runner_->PostTask(FROM_HERE, std::move(closure));
    DCHECK(success);
  }

  // Sets up the D-Bus connection.
  void InitOnTaskRunner() {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    // |bus_| may be non-null in unit testing where a fake bus is used.
    if (!bus_) {
      dbus::Bus::Options bus_options;
      bus_options.bus_type = dbus::Bus::SESSION;
      bus_options.connection_type = dbus::Bus::PRIVATE;
      bus_options.dbus_task_runner = task_runner_;
      bus_ = make_scoped_refptr(new dbus::Bus(bus_options));
    }

    notification_proxy_ =
        bus_->GetObjectProxy(kFreedesktopNotificationsName,
                             dbus::ObjectPath(kFreedesktopNotificationsPath));
    if (!notification_proxy_) {
      OnConnectionInitializationFinishedOnTaskRunner(false);
      return;
    }

    connected_signals_barrier_ = base::BarrierClosure(
        2, base::Bind(&NotificationPlatformBridgeLinuxImpl::
                          OnConnectionInitializationFinishedOnTaskRunner,
                      this, true));
    notification_proxy_->ConnectToSignal(
        kFreedesktopNotificationsName, "ActionInvoked",
        base::Bind(&NotificationPlatformBridgeLinuxImpl::OnActionInvoked, this),
        base::Bind(&NotificationPlatformBridgeLinuxImpl::OnSignalConnected,
                   this));
    notification_proxy_->ConnectToSignal(
        kFreedesktopNotificationsName, "NotificationClosed",
        base::Bind(&NotificationPlatformBridgeLinuxImpl::OnNotificationClosed,
                   this),
        base::Bind(&NotificationPlatformBridgeLinuxImpl::OnSignalConnected,
                   this));
  }

  void CleanUpOnTaskRunner() {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    if (bus_)
      bus_->ShutdownAndBlock();
    bus_ = nullptr;
    notification_proxy_ = nullptr;
    notifications_.clear();
  }

  // Makes the "Notify" call to D-Bus.
  void DisplayOnTaskRunner(NotificationCommon::Type notification_type,
                           const std::string& notification_id,
                           const std::string& profile_id,
                           bool is_incognito,
                           std::unique_ptr<Notification> notification) {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    NotificationData* data =
        FindNotificationData(notification_id, profile_id, is_incognito);
    if (data) {
      // Update an existing notification.
      data->notification_type = notification_type;
      data->resource_files.clear();
    } else {
      // Send the notification for the first time.
      data =
          new NotificationData(notification_type, notification_id, profile_id,
                               is_incognito, notification->origin_url());
      notifications_.emplace(data, base::WrapUnique(data));
    }

    dbus::MethodCall method_call(kFreedesktopNotificationsName, "Notify");
    dbus::MessageWriter writer(&method_call);

    // app_name passed implicitly via desktop-entry.
    writer.AppendString("");

    writer.AppendUint32(data->dbus_id);

    // app_icon passed implicitly via desktop-entry.
    writer.AppendString("");

    writer.AppendString(base::UTF16ToUTF8(notification->title()));

    writer.AppendString(base::UTF16ToUTF8(notification->message()));

    // Even-indexed elements in this vector are action IDs passed back to
    // us in OnActionInvoked().  Odd-indexed ones contain the button text.
    std::vector<std::string> actions;
    data->action_start = data->action_end;
    for (const auto& button_info : notification->buttons()) {
      // FDO notification buttons can contain either an icon or a label,
      // but not both, and the type of all buttons must be the same (all
      // labels or all icons), so always use labels.
      const std::string id = base::SizeTToString(data->action_end++);
      const std::string label = base::UTF16ToUTF8(button_info.title);
      actions.push_back(id);
      actions.push_back(label);
    }
    if (notification->clickable()) {
      // Special case: the pair ("default", "") will not add a button,
      // but instead makes the entire notification clickable.
      actions.push_back(kDefaultButtonId);
      actions.push_back("");
    }
    // Always add a settings button.
    actions.push_back(kSettingsButtonId);
    actions.push_back("Settings");
    writer.AppendArrayOfStrings(actions);

    dbus::MessageWriter hints_writer(nullptr);
    writer.OpenArray("{sv}", &hints_writer);
    dbus::MessageWriter urgency_writer(nullptr);
    hints_writer.OpenDictEntry(&urgency_writer);
    urgency_writer.AppendString("urgency");
    urgency_writer.AppendVariantOfUint32(
        NotificationPriorityToFdoUrgency(notification->priority()));
    hints_writer.CloseContainer(&urgency_writer);

    std::unique_ptr<base::Environment> env = base::Environment::Create();
    base::FilePath desktop_file(
        shell_integration_linux::GetDesktopName(env.get()));
    const char kDesktopFileSuffix[] = ".desktop";
    DCHECK(base::EndsWith(desktop_file.value(), kDesktopFileSuffix,
                          base::CompareCase::SENSITIVE));
    desktop_file = desktop_file.RemoveFinalExtension();
    dbus::MessageWriter desktop_entry_writer(nullptr);
    hints_writer.OpenDictEntry(&desktop_entry_writer);
    desktop_entry_writer.AppendString("desktop-entry");
    desktop_entry_writer.AppendVariantOfString(desktop_file.value());
    hints_writer.CloseContainer(&desktop_entry_writer);

    base::FilePath icon_file =
        WriteDataToTmpFile(notification->icon().As1xPNGBytes());
    if (!icon_file.empty()) {
      dbus::MessageWriter image_path_writer(nullptr);
      hints_writer.OpenDictEntry(&image_path_writer);
      image_path_writer.AppendString("image-path");
      image_path_writer.AppendVariantOfString(icon_file.value());
      hints_writer.CloseContainer(&image_path_writer);
      data->resource_files.push_back(base::MakeUnique<ResourceFile>(icon_file));
    }

    writer.CloseContainer(&hints_writer);

    const int32_t kExpireTimeoutDefault = -1;
    writer.AppendInt32(kExpireTimeoutDefault);

    std::unique_ptr<dbus::Response> response =
        notification_proxy_->CallMethodAndBlock(
            &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
    if (response) {
      dbus::MessageReader reader(response.get());
      reader.PopUint32(&data->dbus_id);
    }
    if (!data->dbus_id) {
      // There was some sort of error with creating the notification.
      notifications_.erase(data);
    }
  }

  // Makes the "CloseNotification" call to D-Bus.
  void CloseOnTaskRunner(const std::string& profile_id,
                         const std::string& notification_id) {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    std::vector<NotificationData*> to_erase;
    for (const auto& pair : notifications_) {
      NotificationData* data = pair.first;
      if (data->notification_id == notification_id &&
          data->profile_id == profile_id) {
        dbus::MethodCall method_call(kFreedesktopNotificationsName,
                                     "CloseNotification");
        dbus::MessageWriter writer(&method_call);
        writer.AppendUint32(data->dbus_id);
        notification_proxy_->CallMethodAndBlock(
            &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
      }
    }
    for (NotificationData* data : to_erase)
      notifications_.erase(data);
  }

  void GetDisplayedOnTaskRunner(
      const std::string& profile_id,
      bool incognito,
      const GetDisplayedNotificationsCallback& callback) const {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    auto displayed = base::MakeUnique<std::set<std::string>>();
    for (const auto& notification : notifications_) {
      if (notification.first->profile_id == profile_id &&
          notification.first->is_incognito == incognito) {
        displayed->insert(notification.first->notification_id);
      }
    }
    PostTaskToUiThread(base::BindOnce(callback, std::move(displayed), true));
  }

  NotificationData* FindNotificationData(const std::string& notification_id,
                                         const std::string& profile_id,
                                         bool is_incognito) {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    for (const auto& pair : notifications_) {
      NotificationData* data = pair.first;
      if (data->notification_id == notification_id &&
          data->profile_id == profile_id &&
          data->is_incognito == is_incognito) {
        return data;
      }
    }

    return nullptr;
  }

  NotificationData* FindNotificationData(uint32_t dbus_id) {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    for (const auto& pair : notifications_) {
      NotificationData* data = pair.first;
      if (data->dbus_id == dbus_id)
        return data;
    }

    return nullptr;
  }

  void ForwardNotificationOperation(NotificationData* data,
                                    NotificationCommon::Operation operation,
                                    int action_index) {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    PostTaskToUiThread(base::BindOnce(
        ForwardNotificationOperationOnUiThread, operation,
        data->notification_type, data->origin_url.spec(), data->notification_id,
        action_index, data->profile_id, data->is_incognito));
  }

  void OnActionInvoked(dbus::Signal* signal) {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    dbus::MessageReader reader(signal);
    uint32_t dbus_id;
    if (!reader.PopUint32(&dbus_id))
      return;
    std::string action;
    if (!reader.PopString(&action))
      return;

    NotificationData* data = FindNotificationData(dbus_id);
    if (!data)
      return;

    if (action == kDefaultButtonId) {
      ForwardNotificationOperation(data, NotificationCommon::CLICK, -1);
    } else if (action == kSettingsButtonId) {
      ForwardNotificationOperation(data, NotificationCommon::SETTINGS, -1);
    } else {
      size_t id;
      if (!base::StringToSizeT(action, &id))
        return;
      size_t n_buttons = data->action_end - data->action_start;
      size_t id_zero_based = id - data->action_start;
      if (id_zero_based >= n_buttons)
        return;
      ForwardNotificationOperation(data, NotificationCommon::CLICK,
                                   id_zero_based);
    }
  }

  void OnNotificationClosed(dbus::Signal* signal) {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    dbus::MessageReader reader(signal);
    uint32_t dbus_id;
    if (!reader.PopUint32(&dbus_id))
      return;

    NotificationData* data = FindNotificationData(dbus_id);
    if (!data)
      return;

    ForwardNotificationOperation(data, NotificationCommon::CLOSE, -1);
    notifications_.erase(data);
  }

  // Called once the connection has been set up (or not).  |success|
  // indicates the connection is ready to use.
  void OnConnectionInitializationFinishedOnUiThread(bool success) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    connected_ = success;
    for (auto& callback : on_connected_callbacks_)
      std::move(callback).Run(success);
    on_connected_callbacks_.clear();
    if (!success)
      CleanUp();
  }

  void OnConnectionInitializationFinishedOnTaskRunner(bool success) {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    PostTaskToUiThread(
        base::BindOnce(&NotificationPlatformBridgeLinuxImpl::
                           OnConnectionInitializationFinishedOnUiThread,
                       this, success));
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success) {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());
    if (!success) {
      OnConnectionInitializationFinishedOnTaskRunner(false);
      return;
    }
    connected_signals_barrier_.Run();
  }

  //////////////////////////////////////////////////////////////////////////////
  // Members used only on the UI thread.

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  content::NotificationRegistrar registrar_;

  // State necessary for OnConnectionInitializationFinished() and
  // SetReadyCallback().
  base::Optional<bool> connected_;
  std::vector<NotificationBridgeReadyCallback> on_connected_callbacks_;

  //////////////////////////////////////////////////////////////////////////////
  // Members used only on the task runner thread.

  scoped_refptr<dbus::Bus> bus_;

  dbus::ObjectProxy* notification_proxy_ = nullptr;

  base::Closure connected_signals_barrier_;

  // A std::set<std::unique_ptr<T>> doesn't work well because
  // eg. std::set::erase(T) would require a std::unique_ptr<T>
  // argument, so the data would get double-destructed.
  template <typename T>
  using UnorderedUniqueSet = std::unordered_map<T*, std::unique_ptr<T>>;

  UnorderedUniqueSet<NotificationData> notifications_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeLinuxImpl);
};

NotificationPlatformBridgeLinux::NotificationPlatformBridgeLinux()
    : NotificationPlatformBridgeLinux(nullptr) {}

NotificationPlatformBridgeLinux::NotificationPlatformBridgeLinux(
    scoped_refptr<dbus::Bus> bus)
    : impl_(new NotificationPlatformBridgeLinuxImpl(bus)) {
  impl_->Init();
}

NotificationPlatformBridgeLinux::~NotificationPlatformBridgeLinux() = default;

void NotificationPlatformBridgeLinux::Display(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    const std::string& profile_id,
    bool is_incognito,
    const Notification& notification) {
  impl_->Display(notification_type, notification_id, profile_id, is_incognito,
                 notification);
}

void NotificationPlatformBridgeLinux::Close(
    const std::string& profile_id,
    const std::string& notification_id) {
  impl_->Close(profile_id, notification_id);
}

void NotificationPlatformBridgeLinux::GetDisplayed(
    const std::string& profile_id,
    bool incognito,
    const GetDisplayedNotificationsCallback& callback) const {
  impl_->GetDisplayed(profile_id, incognito, callback);
}

void NotificationPlatformBridgeLinux::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  impl_->SetReadyCallback(std::move(callback));
}

void NotificationPlatformBridgeLinux::CleanUp() {
  impl_->CleanUp();
}
