// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/dbus/gesture_properties_service_provider.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ash {

GesturePropertiesServiceProvider::GesturePropertiesServiceProvider()
    : weak_ptr_factory_(this) {}

GesturePropertiesServiceProvider::~GesturePropertiesServiceProvider() = default;

void GesturePropertiesServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  dbus::ExportedObject::OnExportedCallback on_exported =
      base::BindRepeating(&GesturePropertiesServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr());
  exported_object->ExportMethod(
      chromeos::kGesturePropertiesServiceInterface,
      chromeos::kGesturePropertiesServiceListDevicesMethod,
      base::BindRepeating(&GesturePropertiesServiceProvider::ListDevices,
                          weak_ptr_factory_.GetWeakPtr()),
      on_exported);
  exported_object->ExportMethod(
      chromeos::kGesturePropertiesServiceInterface,
      chromeos::kGesturePropertiesServiceListPropertiesMethod,
      base::BindRepeating(&GesturePropertiesServiceProvider::ListProperties,
                          weak_ptr_factory_.GetWeakPtr()),
      on_exported);
  exported_object->ExportMethod(
      chromeos::kGesturePropertiesServiceInterface,
      chromeos::kGesturePropertiesServiceGetPropertyMethod,
      base::BindRepeating(&GesturePropertiesServiceProvider::GetProperty,
                          weak_ptr_factory_.GetWeakPtr()),
      on_exported);
}

void GesturePropertiesServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (!success)
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
}

namespace {

void GetPropertyCallback(
    dbus::MethodCall* method_call,
    std::unique_ptr<dbus::Response> response,
    const dbus::ExportedObject::ResponseSender& response_sender,
    bool is_read_only,
    ui::ozone::mojom::GesturePropValuePtr values) {
  if (values.is_null()) {
    response_sender.Run(dbus::ErrorResponse::FromMethodCall(
        method_call, DBUS_ERROR_INVALID_ARGS,
        "The device ID or property name specified was not found."));
    return;
  }

  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter variant_writer(nullptr);
  dbus::MessageWriter array_writer(nullptr);
  writer.AppendBool(is_read_only);
  switch (values->which()) {
    case ui::ozone::mojom::GesturePropValue::Tag::INTS: {
      writer.AppendUint32(values->get_ints().size());
      writer.OpenVariant("ai", &variant_writer);
      variant_writer.AppendArrayOfInt32s(values->get_ints().data(),
                                         values->get_ints().size());
      writer.CloseContainer(&variant_writer);
      break;
    }
    case ui::ozone::mojom::GesturePropValue::Tag::SHORTS: {
      writer.AppendUint32(values->get_shorts().size());
      writer.OpenVariant("an", &variant_writer);
      variant_writer.OpenArray("n", &array_writer);
      for (int16_t value : values->get_shorts()) {
        array_writer.AppendInt16(value);
      }
      variant_writer.CloseContainer(&array_writer);
      writer.CloseContainer(&variant_writer);
      break;
    }
    case ui::ozone::mojom::GesturePropValue::Tag::BOOLS: {
      writer.AppendUint32(values->get_bools().size());
      writer.OpenVariant("ab", &variant_writer);
      variant_writer.OpenArray("b", &array_writer);
      for (bool value : values->get_bools()) {
        array_writer.AppendBool(value);
      }
      variant_writer.CloseContainer(&array_writer);
      writer.CloseContainer(&variant_writer);
      break;
    }
    case ui::ozone::mojom::GesturePropValue::Tag::STR: {
      writer.AppendUint32(1);
      writer.AppendVariantOfString(values->get_str());
      break;
    }
    case ui::ozone::mojom::GesturePropValue::Tag::REALS: {
      writer.AppendUint32(values->get_reals().size());
      writer.OpenVariant("ad", &variant_writer);
      variant_writer.AppendArrayOfDoubles(values->get_reals().data(),
                                          values->get_reals().size());
      writer.CloseContainer(&variant_writer);
      break;
    }
    default: {
      // This should never happen.
      LOG(WARNING) << "No value set on GesturePropValue union; not returning "
                      "values to GetProperty call.";
      writer.AppendUint32(0);
      break;
    }
  }
  response_sender.Run(std::move(response));
}

void ListDevicesCallback(
    std::unique_ptr<dbus::Response> response,
    const dbus::ExportedObject::ResponseSender& response_sender,
    const base::flat_map<int, std::string>& result) {
  dbus::MessageWriter writer(response.get());
  writer.AppendUint32(result.size());
  dbus::MessageWriter dict_writer(nullptr);
  writer.OpenArray("{is}", &dict_writer);
  for (const auto& pair : result) {
    dbus::MessageWriter dict_entry_writer(nullptr);
    dict_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendInt32(pair.first);
    dict_entry_writer.AppendString(pair.second);
    dict_writer.CloseContainer(&dict_entry_writer);
  }
  writer.CloseContainer(&dict_writer);
  response_sender.Run(std::move(response));
}

void ListPropertiesCallback(
    std::unique_ptr<dbus::Response> response,
    const dbus::ExportedObject::ResponseSender& response_sender,
    const std::vector<std::string>& result) {
  dbus::MessageWriter writer(response.get());
  writer.AppendUint32(result.size());
  writer.AppendArrayOfStrings(result);
  response_sender.Run(std::move(response));
}

}  // namespace

void GesturePropertiesServiceProvider::ListDevices(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  GetService()->ListDevices(base::BindOnce(
      &ListDevicesCallback, std::move(response), response_sender));
}

void GesturePropertiesServiceProvider::ListProperties(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  int32_t device_id;
  if (!reader.PopInt32(&device_id)) {
    response_sender.Run(dbus::ErrorResponse::FromMethodCall(
        method_call, DBUS_ERROR_INVALID_ARGS,
        "The device ID (int32) is missing."));
    return;
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  GetService()->ListProperties(
      device_id, base::BindOnce(&ListPropertiesCallback, std::move(response),
                                response_sender));
}

void GesturePropertiesServiceProvider::GetProperty(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  int32_t device_id;
  std::string property_name;

  if (!reader.PopInt32(&device_id) || !reader.PopString(&property_name)) {
    response_sender.Run(dbus::ErrorResponse::FromMethodCall(
        method_call, DBUS_ERROR_INVALID_ARGS,
        "The device ID (int32) and/or property name (string) is missing."));
    return;
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  GetService()->GetProperty(
      device_id, property_name,
      base::BindOnce(&GetPropertyCallback, method_call, std::move(response),
                     response_sender));
}

ui::ozone::mojom::GesturePropertiesService*
GesturePropertiesServiceProvider::GetService() {
  if (service_for_test_ != nullptr)
    return service_for_test_;

  if (!service_.is_bound()) {
    ui::ozone::mojom::GesturePropertiesServiceRequest request =
        mojo::MakeRequest(&service_);
    ui::OzonePlatform::GetInstance()
        ->GetInputController()
        ->GetGesturePropertiesService(std::move(request));
  }
  return service_.get();
}

}  // namespace ash
