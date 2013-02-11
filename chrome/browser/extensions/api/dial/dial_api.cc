// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/dial/dial_api.h"

#include <vector>

#include "base/time.h"
#include "chrome/browser/extensions/api/dial/dial_api_factory.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/dial.h"
#include "content/public/browser/browser_thread.h"

using base::TimeDelta;

namespace {

const char kDialServiceError[] = "Dial service error.";

// How often to poll for devices.
const int kDialRefreshIntervalSecs = 120;

// We prune a device if it does not respond after this time.
const int kDialExpirationSecs = 240;

// The maximum number of devices retained at once in the registry.
const size_t kDialMaxDevices = 256;

}  // namespace

namespace extensions {

DialAPI::DialAPI(Profile* profile)
    : RefcountedProfileKeyedService(content::BrowserThread::IO),
      profile_(profile) {
  ExtensionSystem::Get(profile)->event_router()->RegisterObserver(
      this, extensions::event_names::kOnDialDeviceList);
}

DialAPI::~DialAPI() {}

DialRegistry* DialAPI::dial_registry() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!dial_registry_.get()) {
    dial_registry_.reset(new DialRegistry(this,
        TimeDelta::FromSeconds(kDialRefreshIntervalSecs),
        TimeDelta::FromSeconds(kDialExpirationSecs),
        kDialMaxDevices));
  }
  return dial_registry_.get();
}

void DialAPI::OnListenerAdded(const EventListenerInfo& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DialAPI::NotifyListenerAddedOnIOThread, this));
}

void DialAPI::OnListenerRemoved(const EventListenerInfo& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DialAPI::NotifyListenerRemovedOnIOThread, this));
}

void DialAPI::NotifyListenerAddedOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "DIAL device event listener added.";
  dial_registry()->OnListenerAdded();
}

void DialAPI::NotifyListenerRemovedOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "DIAL device event listener removed";
  dial_registry()->OnListenerRemoved();
}

void DialAPI::OnDialDeviceEvent(const DialRegistry::DeviceList& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&DialAPI::SendEventOnUIThread, this, devices));
}

void DialAPI::OnDialError(const DialRegistry::DialErrorCode code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&DialAPI::SendErrorOnUIThread, this, code));
}

void DialAPI::SendEventOnUIThread(const DialRegistry::DeviceList& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<linked_ptr<api::dial::DialDevice> > args;
  for (DialRegistry::DeviceList::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    linked_ptr<api::dial::DialDevice> api_device =
        make_linked_ptr(new api::dial::DialDevice);
    it->FillDialDevice(api_device.get());
    args.push_back(api_device);
  }
  scoped_ptr<base::ListValue> results = api::dial::OnDeviceList::Create(args);
  scoped_ptr<Event> event(
      new Event(event_names::kOnDialDeviceList, results.Pass()));
  extensions::ExtensionSystem::Get(profile_)->event_router()->
      BroadcastEvent(event.Pass());
}

void DialAPI::SendErrorOnUIThread(const DialRegistry::DialErrorCode code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  api::dial::DialError dial_error;
  switch (code) {
    case DialRegistry::DIAL_NO_LISTENERS:
      dial_error.code = api::dial::DIAL_ERROR_CODE_NO_LISTENERS;
      break;
    case DialRegistry::DIAL_NO_INTERFACES:
      dial_error.code = api::dial::DIAL_ERROR_CODE_NO_VALID_NETWORK_INTERFACES;
      break;
    case DialRegistry::DIAL_CELLULAR_NETWORK:
      dial_error.code = api::dial::DIAL_ERROR_CODE_CELLULAR_NETWORK;
      break;
    case DialRegistry::DIAL_NETWORK_DISCONNECTED:
      dial_error.code = api::dial::DIAL_ERROR_CODE_NETWORK_DISCONNECTED;
      break;
    case DialRegistry::DIAL_SOCKET_ERROR:
      dial_error.code = api::dial::DIAL_ERROR_CODE_SOCKET_ERROR;
      break;
    default:
      dial_error.code = api::dial::DIAL_ERROR_CODE_UNKNOWN;
      break;
  }

  scoped_ptr<base::ListValue> results = api::dial::OnError::Create(dial_error);
  scoped_ptr<Event> event(new Event(event_names::kOnDialError, results.Pass()));
  extensions::ExtensionSystem::Get(profile_)->event_router()->
      BroadcastEvent(event.Pass());
}

void DialAPI::ShutdownOnUIThread() {}

namespace api {

DialDiscoverNowFunction::DialDiscoverNowFunction()
    : dial_(NULL), result_(false) {
}

bool DialDiscoverNowFunction::Prepare() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile());
  dial_ = DialAPIFactory::GetInstance()->GetForProfile(profile());
  return true;
}

void DialDiscoverNowFunction::Work() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  result_ = dial_->dial_registry()->DiscoverNow();
}

bool DialDiscoverNowFunction::Respond() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!result_)
    error_ = kDialServiceError;

  SetResult(base::Value::CreateBooleanValue(result_));
  return true;
}

}  // namespace api

}  // namespace extensions
