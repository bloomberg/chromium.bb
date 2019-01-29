// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_chooser_context.h"

#include <utility>

#include "base/base64.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/common/service_manager_connection.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

constexpr char kPortNameKey[] = "name";
constexpr char kTokenKey[] = "token";

std::string EncodeToken(const base::UnguessableToken& token) {
  const uint64_t data[2] = {token.GetHighForSerialization(),
                            token.GetLowForSerialization()};
  std::string buffer;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(&data[0]), sizeof(data)),
      &buffer);
  return buffer;
}

base::UnguessableToken DecodeToken(base::StringPiece input) {
  std::string buffer;
  if (!base::Base64Decode(input, &buffer) ||
      buffer.length() != sizeof(uint64_t) * 2) {
    return base::UnguessableToken();
  }

  const uint64_t* data = reinterpret_cast<const uint64_t*>(buffer.data());
  return base::UnguessableToken::Deserialize(data[0], data[1]);
}

base::Value PortInfoToValue(const device::mojom::SerialPortInfo& port) {
  base::Value value(base::Value::Type::DICTIONARY);
  if (port.display_name)
    value.SetKey(kPortNameKey, base::Value(*port.display_name));
  else
    value.SetKey(kPortNameKey, base::Value(port.path.LossyDisplayName()));
  value.SetKey(kTokenKey, base::Value(EncodeToken(port.token)));
  return value;
}

}  // namespace

SerialChooserContext::SerialChooserContext(Profile* profile)
    : ChooserContextBase(profile,
                         CONTENT_SETTINGS_TYPE_SERIAL_GUARD,
                         CONTENT_SETTINGS_TYPE_SERIAL_CHOOSER_DATA),
      is_incognito_(profile->IsOffTheRecord()) {}

SerialChooserContext::~SerialChooserContext() = default;

bool SerialChooserContext::IsValidObject(const base::DictionaryValue& object) {
  const std::string* token = object.FindStringKey(kTokenKey);
  return object.size() == 2 && object.FindStringKey(kPortNameKey) && token &&
         DecodeToken(*token);
}

std::string SerialChooserContext::GetObjectName(
    const base::DictionaryValue& object) {
  DCHECK(IsValidObject(object));
  return *object.FindStringKey(kPortNameKey);
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
SerialChooserContext::GetGrantedObjects(const GURL& requesting_origin,
                                        const GURL& embedding_origin) {
  std::vector<std::unique_ptr<Object>> objects;
  auto origin_it = ephemeral_ports_.find(
      std::make_pair(url::Origin::Create(requesting_origin),
                     url::Origin::Create(embedding_origin)));
  if (origin_it == ephemeral_ports_.end())
    return objects;
  const std::set<base::UnguessableToken> ports = origin_it->second;

  for (const auto& token : ports) {
    auto it = port_info_.find(token);
    if (it == port_info_.end())
      continue;

    // Object's constructor should take a base::Value directly.
    base::Value clone = it->second.Clone();
    base::DictionaryValue* object;
    clone.GetAsDictionary(&object);

    objects.push_back(std::make_unique<Object>(
        requesting_origin, embedding_origin, object,
        content_settings::SettingSource::SETTING_SOURCE_USER, is_incognito_));
  }

  return objects;
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
SerialChooserContext::GetAllGrantedObjects() {
  std::vector<std::unique_ptr<Object>> objects;
  for (const auto& map_entry : ephemeral_ports_) {
    GURL requesting_origin = map_entry.first.first.GetURL();
    GURL embedding_origin = map_entry.first.second.GetURL();

    if (!CanRequestObjectPermission(requesting_origin, embedding_origin))
      continue;

    for (const auto& token : map_entry.second) {
      auto it = port_info_.find(token);
      if (it == port_info_.end())
        continue;

      // Object's constructor should take a base::Value directly.
      base::Value clone = it->second.Clone();
      base::DictionaryValue* object;
      clone.GetAsDictionary(&object);

      objects.push_back(std::make_unique<Object>(
          requesting_origin, embedding_origin, object,
          content_settings::SettingSource::SETTING_SOURCE_USER, is_incognito_));
    }
  }

  return objects;
}

void SerialChooserContext::RevokeObjectPermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::DictionaryValue& object) {
  auto origin_it = ephemeral_ports_.find(
      std::make_pair(url::Origin::Create(requesting_origin),
                     url::Origin::Create(embedding_origin)));
  if (origin_it == ephemeral_ports_.end())
    return;
  std::set<base::UnguessableToken>& ports = origin_it->second;

  DCHECK(IsValidObject(object));
  ports.erase(DecodeToken(*object.FindStringKey(kTokenKey)));
}

void SerialChooserContext::GrantPortPermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const device::mojom::SerialPortInfo& port) {
  // TODO(crbug.com/908836): If |port| can be remembered persistently call into
  // ChooserContextBase to store it in user preferences.
  ephemeral_ports_[std::make_pair(requesting_origin, embedding_origin)].insert(
      port.token);
  port_info_[port.token] = PortInfoToValue(port);
}

bool SerialChooserContext::HasPortPermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const device::mojom::SerialPortInfo& port) {
  if (!CanRequestObjectPermission(requesting_origin.GetURL(),
                                  embedding_origin.GetURL())) {
    return false;
  }

  auto origin_it = ephemeral_ports_.find(
      std::make_pair(requesting_origin, embedding_origin));
  if (origin_it == ephemeral_ports_.end())
    return false;
  const std::set<base::UnguessableToken> ports = origin_it->second;

  // TODO(crbug.com/908836): Call into ChooserContextBase to check persistent
  // permissions.
  auto port_it = ports.find(port.token);
  return port_it != ports.end();
}

device::mojom::SerialPortManager* SerialChooserContext::GetPortManager() {
  EnsurePortManagerConnection();
  return port_manager_.get();
}

void SerialChooserContext::SetPortManagerForTesting(
    device::mojom::SerialPortManagerPtr manager) {
  SetUpPortManagerConnection(std::move(manager));
}

base::WeakPtr<SerialChooserContext> SerialChooserContext::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void SerialChooserContext::EnsurePortManagerConnection() {
  if (port_manager_)
    return;

  device::mojom::SerialPortManagerPtr manager;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(device::mojom::kServiceName, mojo::MakeRequest(&manager));
  SetUpPortManagerConnection(std::move(manager));
}

void SerialChooserContext::SetUpPortManagerConnection(
    device::mojom::SerialPortManagerPtr manager) {
  port_manager_ = std::move(manager);
  port_manager_.set_connection_error_handler(
      base::BindOnce(&SerialChooserContext::OnPortManagerConnectionError,
                     base::Unretained(this)));
}

void SerialChooserContext::OnPortManagerConnectionError() {
  port_info_.clear();
  ephemeral_ports_.clear();
}
