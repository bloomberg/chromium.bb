// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device/input_service_proxy.h"

#include "base/bind_helpers.h"
#include "base/task_runner_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;
using device::InputServiceLinux;

typedef device::InputServiceLinux::InputDeviceInfo InputDeviceInfo;

namespace chromeos {

class InputServiceProxy::ServiceObserver : public InputServiceLinux::Observer {
 public:
  ServiceObserver() { DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI)); }
  virtual ~ServiceObserver() { DCHECK(CalledOnValidThread()); }

  void Initialize(const base::WeakPtr<InputServiceProxy>& proxy) {
    DCHECK(CalledOnValidThread());
    InputServiceLinux::GetInstance()->AddObserver(this);
    proxy_ = proxy;
  }

  void Shutdown() {
    DCHECK(CalledOnValidThread());
    if (InputServiceLinux::HasInstance())
      InputServiceLinux::GetInstance()->RemoveObserver(this);
    delete this;
  }

  std::vector<InputDeviceInfo> GetDevices() {
    DCHECK(CalledOnValidThread());
    std::vector<InputDeviceInfo> devices;
    if (InputServiceLinux::HasInstance())
      InputServiceLinux::GetInstance()->GetDevices(&devices);
    return devices;
  }

  void GetDeviceInfo(const std::string& id,
                     const InputServiceProxy::GetDeviceInfoCallback& callback) {
    DCHECK(CalledOnValidThread());
    bool success = false;
    InputDeviceInfo info;
    info.id = id;
    if (InputServiceLinux::HasInstance())
      success = InputServiceLinux::GetInstance()->GetDeviceInfo(id, &info);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::Bind(callback, success, info));
  }

  // InputServiceLinux::Observer implementation:
  virtual void OnInputDeviceAdded(
      const InputServiceLinux::InputDeviceInfo& info) OVERRIDE {
    DCHECK(CalledOnValidThread());
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&InputServiceProxy::OnDeviceAdded, proxy_, info));
  }

  virtual void OnInputDeviceRemoved(const std::string& id) OVERRIDE {
    DCHECK(CalledOnValidThread());
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&InputServiceProxy::OnDeviceRemoved, proxy_, id));
  }

 private:
  bool CalledOnValidThread() const {
    return BrowserThread::CurrentlyOn(BrowserThread::FILE);
  }

  base::WeakPtr<InputServiceProxy> proxy_;

  DISALLOW_COPY_AND_ASSIGN(ServiceObserver);
};

InputServiceProxy::InputServiceProxy()
    : service_observer_(new ServiceObserver()), weak_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&InputServiceProxy::ServiceObserver::Initialize,
                 base::Unretained(service_observer_.get()),
                 weak_factory_.GetWeakPtr()));
}

InputServiceProxy::~InputServiceProxy() {
  DCHECK(thread_checker_.CalledOnValidThread());
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&InputServiceProxy::ServiceObserver::Shutdown,
                 base::Unretained(service_observer_.release())));
}

// static
void InputServiceProxy::WarmUp() {
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&InputServiceLinux::GetInstance)));
}

void InputServiceProxy::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (observer)
    observers_.AddObserver(observer);
}

void InputServiceProxy::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (observer)
    observers_.RemoveObserver(observer);
}

void InputServiceProxy::GetDevices(const GetDevicesCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&InputServiceProxy::ServiceObserver::GetDevices,
                 base::Unretained(service_observer_.get())),
      callback);
}

void InputServiceProxy::GetDeviceInfo(const std::string& id,
                                      const GetDeviceInfoCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&InputServiceProxy::ServiceObserver::GetDeviceInfo,
                 base::Unretained(service_observer_.release()),
                 id,
                 callback));
}

void InputServiceProxy::OnDeviceAdded(
    const InputServiceLinux::InputDeviceInfo& info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FOR_EACH_OBSERVER(Observer, observers_, OnInputDeviceAdded(info));
}

void InputServiceProxy::OnDeviceRemoved(const std::string& id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FOR_EACH_OBSERVER(Observer, observers_, OnInputDeviceRemoved(id));
}

}  // namespace chromeos
