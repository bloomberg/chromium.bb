// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wl_data_device_manager.h"

#include <wayland-server-protocol-core.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "components/exo/data_device.h"
#include "components/exo/data_device_delegate.h"
#include "components/exo/data_offer.h"
#include "components/exo/data_offer_delegate.h"
#include "components/exo/data_source.h"
#include "components/exo/data_source_delegate.h"
#include "components/exo/display.h"
#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {
namespace {

uint32_t WaylandDataDeviceManagerDndAction(DndAction action) {
  switch (action) {
    case DndAction::kNone:
      return WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
    case DndAction::kCopy:
      return WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    case DndAction::kMove:
      return WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    case DndAction::kAsk:
      return WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
  }
  NOTREACHED();
}

uint32_t WaylandDataDeviceManagerDndActions(
    const base::flat_set<DndAction>& dnd_actions) {
  uint32_t actions = 0;
  for (DndAction action : dnd_actions)
    actions |= WaylandDataDeviceManagerDndAction(action);
  return actions;
}

DndAction DataDeviceManagerDndAction(uint32_t value) {
  switch (value) {
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE:
      return DndAction::kNone;
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY:
      return DndAction::kCopy;
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE:
      return DndAction::kMove;
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK:
      return DndAction::kAsk;
    default:
      NOTREACHED();
      return DndAction::kNone;
  }
}

base::flat_set<DndAction> DataDeviceManagerDndActions(uint32_t value) {
  base::flat_set<DndAction> actions;
  if (value & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
    actions.insert(DndAction::kCopy);
  if (value & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
    actions.insert(DndAction::kMove);
  if (value & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)
    actions.insert(DndAction::kAsk);
  return actions;
}

////////////////////////////////////////////////////////////////////////////////
// wl_data_source_interface:

class WaylandDataSourceDelegate : public DataSourceDelegate {
 public:
  explicit WaylandDataSourceDelegate(wl_resource* source)
      : data_source_resource_(source) {}

  // Overridden from DataSourceDelegate:
  void OnDataSourceDestroying(DataSource* device) override { delete this; }
  void OnTarget(const std::string& mime_type) override {
    wl_data_source_send_target(data_source_resource_, mime_type.c_str());
    wl_client_flush(wl_resource_get_client(data_source_resource_));
  }
  void OnSend(const std::string& mime_type, base::ScopedFD fd) override {
    wl_data_source_send_send(data_source_resource_, mime_type.c_str(),
                             fd.get());
    wl_client_flush(wl_resource_get_client(data_source_resource_));
  }
  void OnCancelled() override {
    wl_data_source_send_cancelled(data_source_resource_);
    wl_client_flush(wl_resource_get_client(data_source_resource_));
  }
  void OnDndDropPerformed() override {
    if (wl_resource_get_version(data_source_resource_) >=
        WL_DATA_SOURCE_DND_DROP_PERFORMED_SINCE_VERSION) {
      wl_data_source_send_dnd_drop_performed(data_source_resource_);
      wl_client_flush(wl_resource_get_client(data_source_resource_));
    }
  }
  void OnDndFinished() override {
    if (wl_resource_get_version(data_source_resource_) >=
        WL_DATA_SOURCE_DND_FINISHED_SINCE_VERSION) {
      wl_data_source_send_dnd_finished(data_source_resource_);
      wl_client_flush(wl_resource_get_client(data_source_resource_));
    }
  }
  void OnAction(DndAction dnd_action) override {
    if (wl_resource_get_version(data_source_resource_) >=
        WL_DATA_SOURCE_ACTION_SINCE_VERSION) {
      wl_data_source_send_action(data_source_resource_,
                                 WaylandDataDeviceManagerDndAction(dnd_action));
      wl_client_flush(wl_resource_get_client(data_source_resource_));
    }
  }

 private:
  wl_resource* const data_source_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataSourceDelegate);
};

void data_source_offer(wl_client* client,
                       wl_resource* resource,
                       const char* mime_type) {
  GetUserDataAs<DataSource>(resource)->Offer(mime_type);
}

void data_source_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void data_source_set_actions(wl_client* client,
                             wl_resource* resource,
                             uint32_t dnd_actions) {
  GetUserDataAs<DataSource>(resource)->SetActions(
      DataDeviceManagerDndActions(dnd_actions));
}

const struct wl_data_source_interface data_source_implementation = {
    data_source_offer, data_source_destroy, data_source_set_actions};

////////////////////////////////////////////////////////////////////////////////
// wl_data_offer_interface:

class WaylandDataOfferDelegate : public DataOfferDelegate {
 public:
  explicit WaylandDataOfferDelegate(wl_resource* offer)
      : data_offer_resource_(offer) {}

  // Overridden from DataOfferDelegate:
  void OnDataOfferDestroying(DataOffer* device) override { delete this; }
  void OnOffer(const std::string& mime_type) override {
    wl_data_offer_send_offer(data_offer_resource_, mime_type.c_str());
    wl_client_flush(wl_resource_get_client(data_offer_resource_));
  }
  void OnSourceActions(
      const base::flat_set<DndAction>& source_actions) override {
    if (wl_resource_get_version(data_offer_resource_) >=
        WL_DATA_OFFER_SOURCE_ACTIONS_SINCE_VERSION) {
      wl_data_offer_send_source_actions(
          data_offer_resource_,
          WaylandDataDeviceManagerDndActions(source_actions));
      wl_client_flush(wl_resource_get_client(data_offer_resource_));
    }
  }
  void OnAction(DndAction action) override {
    if (wl_resource_get_version(data_offer_resource_) >=
        WL_DATA_OFFER_ACTION_SINCE_VERSION) {
      wl_data_offer_send_action(data_offer_resource_,
                                WaylandDataDeviceManagerDndAction(action));
      wl_client_flush(wl_resource_get_client(data_offer_resource_));
    }
  }

 private:
  wl_resource* const data_offer_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataOfferDelegate);
};

void data_offer_accept(wl_client* client,
                       wl_resource* resource,
                       uint32_t serial,
                       const char* mime_type) {
  if (mime_type == nullptr) {
    GetUserDataAs<DataOffer>(resource)->Accept(nullptr);
    return;
  }
  const std::string mime_type_string(mime_type);
  GetUserDataAs<DataOffer>(resource)->Accept(&mime_type_string);
}

void data_offer_receive(wl_client* client,
                        wl_resource* resource,
                        const char* mime_type,
                        int fd) {
  GetUserDataAs<DataOffer>(resource)->Receive(mime_type, base::ScopedFD(fd));
}

void data_offer_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void data_offer_finish(wl_client* client, wl_resource* resource) {
  GetUserDataAs<DataOffer>(resource)->Finish();
}

void data_offer_set_actions(wl_client* client,
                            wl_resource* resource,
                            uint32_t dnd_actions,
                            uint32_t preferred_action) {
  GetUserDataAs<DataOffer>(resource)->SetActions(
      DataDeviceManagerDndActions(dnd_actions),
      DataDeviceManagerDndAction(preferred_action));
}

const struct wl_data_offer_interface data_offer_implementation = {
    data_offer_accept, data_offer_receive, data_offer_finish,
    data_offer_destroy, data_offer_set_actions};

////////////////////////////////////////////////////////////////////////////////
// wl_data_device_interface:

class WaylandDataDeviceDelegate : public DataDeviceDelegate {
 public:
  WaylandDataDeviceDelegate(wl_client* client, wl_resource* device_resource)
      : client_(client), data_device_resource_(device_resource) {}

  // Overridden from DataDeviceDelegate:
  void OnDataDeviceDestroying(DataDevice* device) override { delete this; }
  bool CanAcceptDataEventsForSurface(Surface* surface) override {
    return surface &&
           wl_resource_get_client(GetSurfaceResource(surface)) == client_;
  }
  DataOffer* OnDataOffer() override {
    wl_resource* data_offer_resource =
        wl_resource_create(client_, &wl_data_offer_interface,
                           wl_resource_get_version(data_device_resource_), 0);
    std::unique_ptr<DataOffer> data_offer = std::make_unique<DataOffer>(
        new WaylandDataOfferDelegate(data_offer_resource));
    SetDataOfferResource(data_offer.get(), data_offer_resource);
    SetImplementation(data_offer_resource, &data_offer_implementation,
                      std::move(data_offer));

    wl_data_device_send_data_offer(data_device_resource_, data_offer_resource);
    wl_client_flush(client_);

    return GetUserDataAs<DataOffer>(data_offer_resource);
  }
  void OnEnter(Surface* surface,
               const gfx::PointF& point,
               const DataOffer& data_offer) override {
    wl_data_device_send_enter(
        data_device_resource_,
        wl_display_next_serial(wl_client_get_display(client_)),
        GetSurfaceResource(surface), wl_fixed_from_double(point.x()),
        wl_fixed_from_double(point.y()), GetDataOfferResource(&data_offer));
    wl_client_flush(client_);
  }
  void OnLeave() override {
    wl_data_device_send_leave(data_device_resource_);
    wl_client_flush(client_);
  }
  void OnMotion(base::TimeTicks time_stamp, const gfx::PointF& point) override {
    wl_data_device_send_motion(
        data_device_resource_, TimeTicksToMilliseconds(time_stamp),
        wl_fixed_from_double(point.x()), wl_fixed_from_double(point.y()));
    wl_client_flush(client_);
  }
  void OnDrop() override {
    wl_data_device_send_drop(data_device_resource_);
    wl_client_flush(client_);
  }
  void OnSelection(const DataOffer& data_offer) override {
    wl_data_device_send_selection(data_device_resource_,
                                  GetDataOfferResource(&data_offer));
    wl_client_flush(client_);
  }

 private:
  wl_client* const client_;
  wl_resource* const data_device_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataDeviceDelegate);
};

void data_device_start_drag(wl_client* client,
                            wl_resource* resource,
                            wl_resource* source_resource,
                            wl_resource* origin_resource,
                            wl_resource* icon_resource,
                            uint32_t serial) {
  GetUserDataAs<DataDevice>(resource)->StartDrag(
      source_resource ? GetUserDataAs<DataSource>(source_resource) : nullptr,
      GetUserDataAs<Surface>(origin_resource),
      icon_resource ? GetUserDataAs<Surface>(icon_resource) : nullptr, serial);
}

void data_device_set_selection(wl_client* client,
                               wl_resource* resource,
                               wl_resource* data_source,
                               uint32_t serial) {
  GetUserDataAs<DataDevice>(resource)->SetSelection(
      data_source ? GetUserDataAs<DataSource>(data_source) : nullptr, serial);
}

void data_device_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_data_device_interface data_device_implementation = {
    data_device_start_drag, data_device_set_selection, data_device_release};

////////////////////////////////////////////////////////////////////////////////
// wl_data_device_manager_interface:

void data_device_manager_create_data_source(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t id) {
  wl_resource* data_source_resource = wl_resource_create(
      client, &wl_data_source_interface, wl_resource_get_version(resource), id);
  SetImplementation(data_source_resource, &data_source_implementation,
                    std::make_unique<DataSource>(
                        new WaylandDataSourceDelegate(data_source_resource)));
}

void data_device_manager_get_data_device(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t id,
                                         wl_resource* seat_resource) {
  Display* display = GetUserDataAs<Display>(resource);
  wl_resource* data_device_resource = wl_resource_create(
      client, &wl_data_device_interface, wl_resource_get_version(resource), id);
  SetImplementation(data_device_resource, &data_device_implementation,
                    display->CreateDataDevice(new WaylandDataDeviceDelegate(
                        client, data_device_resource)));
}

const struct wl_data_device_manager_interface
    data_device_manager_implementation = {
        data_device_manager_create_data_source,
        data_device_manager_get_data_device};

}  // namespace

void bind_data_device_manager(wl_client* client,
                              void* data,
                              uint32_t version,
                              uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wl_data_device_manager_interface,
                         std::min(version, kWlDataDeviceManagerVersion), id);
  wl_resource_set_implementation(resource, &data_device_manager_implementation,
                                 data, nullptr);
}

}  // namespace wayland
}  // namespace exo
