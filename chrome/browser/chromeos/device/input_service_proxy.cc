// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device/input_service_proxy.h"

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/lazy_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;
using device::InputServiceLinux;

namespace chromeos {

namespace {

bool use_ui_thread_for_test = false;

// SequencedTaskRunner could be used after InputServiceLinux and friends
// are updated to check on sequence instead of thread.
base::LazySingleThreadTaskRunner default_input_service_task_runner =
    LAZY_SINGLE_THREAD_TASK_RUNNER_INITIALIZER(
        base::TaskTraits({base::TaskPriority::BACKGROUND, base::MayBlock()}),
        base::SingleThreadTaskRunnerThreadMode::SHARED);

}  // namespace

class InputServiceProxy::ServiceObserver : public InputServiceLinux::Observer {
 public:
  ServiceObserver() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // Detach since this object is constructed on UI thread and forever after
    // used from another sequence.
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }
  ~ServiceObserver() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void Initialize(const base::WeakPtr<InputServiceProxy>& proxy) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    InputServiceLinux::GetInstance()->AddObserver(this);
    proxy_ = proxy;
  }

  void Shutdown() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (InputServiceLinux::HasInstance())
      InputServiceLinux::GetInstance()->RemoveObserver(this);
    delete this;
  }

  std::vector<InputDeviceInfoPtr> GetDevices() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::vector<InputDeviceInfoPtr> devices;
    if (InputServiceLinux::HasInstance())
      InputServiceLinux::GetInstance()->GetDevices(&devices);
    return devices;
  }

  // InputServiceLinux::Observer implementation:
  void OnInputDeviceAdded(InputDeviceInfoPtr info) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::BindOnce(&InputServiceProxy::OnDeviceAdded,
                                           proxy_, std::move(info)));
  }

  void OnInputDeviceRemoved(const std::string& id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&InputServiceProxy::OnDeviceRemoved, proxy_, id));
  }

 private:
  base::WeakPtr<InputServiceProxy> proxy_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ServiceObserver);
};

InputServiceProxy::InputServiceProxy()
    : service_observer_(new ServiceObserver()),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetInputServiceTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&InputServiceProxy::ServiceObserver::Initialize,
                                base::Unretained(service_observer_.get()),
                                weak_factory_.GetWeakPtr()));
}

InputServiceProxy::~InputServiceProxy() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetInputServiceTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&InputServiceProxy::ServiceObserver::Shutdown,
                                base::Unretained(service_observer_.release())));
}

void InputServiceProxy::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (observer)
    observers_.AddObserver(observer);
}

void InputServiceProxy::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (observer)
    observers_.RemoveObserver(observer);
}

void InputServiceProxy::GetDevices(GetDevicesCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::PostTaskAndReplyWithResult(
      GetInputServiceTaskRunner().get(), FROM_HERE,
      base::BindOnce(&InputServiceProxy::ServiceObserver::GetDevices,
                     base::Unretained(service_observer_.get())),
      std::move(callback));
}

// static
scoped_refptr<base::SequencedTaskRunner>
InputServiceProxy::GetInputServiceTaskRunner() {
  if (use_ui_thread_for_test)
    return BrowserThread::GetTaskRunnerForThread(BrowserThread::UI);

  return default_input_service_task_runner.Get();
}

// static
void InputServiceProxy::SetUseUIThreadForTesting(bool use_ui_thread) {
  use_ui_thread_for_test = use_ui_thread;
}

void InputServiceProxy::OnDeviceAdded(InputDeviceInfoPtr info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& observer : observers_)
    observer.OnInputDeviceAdded(info->Clone());
}

void InputServiceProxy::OnDeviceRemoved(const std::string& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& observer : observers_)
    observer.OnInputDeviceRemoved(id);
}

}  // namespace chromeos
