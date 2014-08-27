// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/nfc_debug_ui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "device/nfc/nfc_adapter.h"
#include "device/nfc/nfc_adapter_factory.h"
#include "device/nfc/nfc_peer.h"
#include "device/nfc/nfc_tag.h"
#include "grit/browser_resources.h"

using device::NfcAdapter;
using device::NfcAdapterFactory;
using device::NfcNdefMessage;
using device::NfcNdefRecord;
using device::NfcNdefTagTechnology;
using device::NfcPeer;
using device::NfcTag;
using device::NfcTagTechnology;

namespace chromeos {

namespace {

// Keys for various NFC properties that are used by JS.
const char kAdapterPeersProperty[] = "peers";
const char kAdapterPollingProperty[] = "polling";
const char kAdapterPoweredProperty[] = "powered";
const char kAdapterPresentProperty[] = "present";
const char kAdapterTagsProperty[] = "tags";

const char kPeerIdentifierProperty[] = "identifier";
const char kPeerRecordsProperty[] = "records";

const char kTagIdentifierProperty[] = "identifier";
const char kTagTypeProperty[] = "type";
const char kTagReadOnlyProperty[] = "readOnly";
const char kTagRecordsProperty[] = "records";
const char kTagSupportedProtocolProperty[] = "supportedProtocol";
const char kTagSupportedTechnologiesProperty[] = "supportedTechnologies";
const char kRecordTypeProperty[] = "type";

// Tag types.
const char kTagType1[] = "TYPE-1";
const char kTagType2[] = "TYPE-2";
const char kTagType3[] = "TYPE-3";
const char kTagType4[] = "TYPE-4";
const char kTagTypeUnknown[] = "UNKNOWN";

// NFC transfer protocols.
const char kTagProtocolFelica[] = "FELICA";
const char kTagProtocolIsoDep[] = "ISO-DEP";
const char kTagProtocolJewel[] = "JEWEL";
const char kTagProtocolMifare[] = "MIFARE";
const char kTagProtocolNfcDep[] = "NFC-DEP";
const char kTagProtocolUnknown[] = "UNKNOWN";

// NFC tag technologies.
const char kTagTechnologyNfcA[] = "NFC-A";
const char kTagTechnologyNfcB[] = "NFC-B";
const char kTagTechnologyNfcF[] = "NFC-F";
const char kTagTechnologyNfcV[] = "NFC-V";
const char kTagTechnologyIsoDep[] = "ISO-DEP";
const char kTagTechnologyNdef[] = "NDEF";

// NDEF types.
const char kTypeHandoverCarrier[] = "HANDOVER_CARRIER";
const char kTypeHandoverRequest[] = "HANDOVER_REQUEST";
const char kTypeHandoverSelect[] = "HANDOVER_SELECT";
const char kTypeSmartPoster[] = "SMART_POSTER";
const char kTypeText[] = "TEXT";
const char kTypeURI[] = "URI";
const char kTypeUnknown[] = "UNKNOWN";

// JavaScript callback constants.
const char kInitializeCallback[] = "initialize";
const char kSetAdapterPowerCallback[] = "setAdapterPower";
const char kSetAdapterPollingCallback[] = "setAdapterPolling";
const char kSubmitRecordFormCallback[] = "submitRecordForm";

// Constants for JavaScript functions that we can call.
const char kOnNfcAdapterInfoChangedFunction[] =
    "nfcDebug.NfcDebugUI.onNfcAdapterInfoChanged";
const char kOnNfcAvailabilityDeterminedFunction[] =
    "nfcDebug.NfcDebugUI.onNfcAvailabilityDetermined";
const char kOnNfcPeerDeviceInfoChangedFunction[] =
    "nfcDebug.NfcDebugUI.onNfcPeerDeviceInfoChanged";
const char kOnNfcTagInfoChangedFunction[] =
    "nfcDebug.NfcDebugUI.onNfcTagInfoChanged";
const char kOnSetAdapterPollingFailedFunction[] =
    "nfcDebug.NfcDebugUI.onSetAdapterPollingFailed";
const char kOnSetAdapterPowerFailedFunction[] =
    "nfcDebug.NfcDebugUI.onSetAdapterPowerFailed";
const char kOnSubmitRecordFormFailedFunction[] =
    "nfcDebug.NfcDebugUI.onSubmitRecordFormFailed";

std::string RecordTypeToString(NfcNdefRecord::Type type) {
  switch (type) {
    case NfcNdefRecord::kTypeHandoverCarrier:
      return kTypeHandoverCarrier;
    case NfcNdefRecord::kTypeHandoverRequest:
      return kTypeHandoverRequest;
    case NfcNdefRecord::kTypeHandoverSelect:
      return kTypeHandoverSelect;
    case NfcNdefRecord::kTypeSmartPoster:
      return kTypeSmartPoster;
    case NfcNdefRecord::kTypeText:
      return kTypeText;
    case NfcNdefRecord::kTypeURI:
      return kTypeURI;
    case NfcNdefRecord::kTypeUnknown:
      return kTypeUnknown;
  }
  return kTypeUnknown;
}

NfcNdefRecord::Type RecordTypeStringValueToEnum(const std::string& type) {
  if (type == kTypeHandoverCarrier)
    return NfcNdefRecord::kTypeHandoverCarrier;
  if (type == kTypeHandoverRequest)
    return NfcNdefRecord::kTypeHandoverRequest;
  if (type == kTypeHandoverSelect)
    return NfcNdefRecord::kTypeHandoverSelect;
  if (type == kTypeSmartPoster)
    return NfcNdefRecord::kTypeSmartPoster;
  if (type == kTypeText)
    return NfcNdefRecord::kTypeText;
  if (type == kTypeURI)
    return NfcNdefRecord::kTypeURI;
  return NfcNdefRecord::kTypeUnknown;
}

std::string TagTypeToString(NfcTag::TagType type) {
  switch (type) {
    case NfcTag::kTagType1:
      return kTagType1;
    case NfcTag::kTagType2:
      return kTagType2;
    case NfcTag::kTagType3:
      return kTagType3;
    case NfcTag::kTagType4:
      return kTagType4;
    case NfcTag::kTagTypeUnknown:
      return kTagTypeUnknown;
  }
  return kTagTypeUnknown;
}

std::string TagProtocolToString(NfcTag::Protocol protocol) {
  switch (protocol) {
    case NfcTag::kProtocolFelica:
      return kTagProtocolFelica;
    case NfcTag::kProtocolIsoDep:
      return kTagProtocolIsoDep;
    case NfcTag::kProtocolJewel:
      return kTagProtocolJewel;
    case NfcTag::kProtocolMifare:
      return kTagProtocolMifare;
    case NfcTag::kProtocolNfcDep:
      return kTagProtocolNfcDep;
    case NfcTag::kProtocolUnknown:
      return kTagProtocolUnknown;
  }
  return kTagProtocolUnknown;
}

// content::WebUIMessageHandler implementation for this page.
class NfcDebugMessageHandler : public content::WebUIMessageHandler,
                               public NfcAdapter::Observer,
                               public NfcNdefTagTechnology::Observer,
                               public NfcPeer::Observer,
                               public NfcTag::Observer {
 public:
  NfcDebugMessageHandler();
  virtual ~NfcDebugMessageHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // NfcAdapter::Observer overrides.
  virtual void AdapterPresentChanged(
      NfcAdapter* adapter,
      bool present) OVERRIDE;
  virtual void AdapterPoweredChanged(
      NfcAdapter* adapter,
      bool powered) OVERRIDE;
  virtual void AdapterPollingChanged(
      NfcAdapter* adapter,
      bool polling) OVERRIDE;
  virtual void TagFound(NfcAdapter* adapter, NfcTag* tag) OVERRIDE;
  virtual void TagLost(NfcAdapter*adapter, NfcTag* tag) OVERRIDE;
  virtual void PeerFound(NfcAdapter* adaper, NfcPeer* peer) OVERRIDE;
  virtual void PeerLost(NfcAdapter* adapter, NfcPeer* peer) OVERRIDE;

  // NfcNdefTagTechnology::Observer override.
  virtual void RecordReceived(
      NfcTag* tag,
      const NfcNdefRecord* record) OVERRIDE;

  // NfcPeer::Observer override.
  virtual void RecordReceived(
      NfcPeer* peer,
      const NfcNdefRecord* record) OVERRIDE;

  // NfcTag::Observer override.
  virtual void TagReady(NfcTag* tag) OVERRIDE;

 private:
  // Called by the UI when the page loads. This method requests information
  // about NFC availability on the current platform and requests the underlying
  // Adapter object. The UI is notified once the information is available.
  void Initialize(const base::ListValue* args);

  // Called by the UI to toggle the adapter power. |args| will contain one
  // boolean that indicates whether the power should be set to ON or OFF.
  void SetAdapterPower(const base::ListValue* args);
  void OnSetAdapterPowerError();

  // Called by the UI set the adapter's poll status. |args| will contain one
  // boolean that indicates whether polling should be started or stopped.
  void SetAdapterPolling(const base::ListValue* args);
  void OnSetAdapterPollingError();

  // Called by the UI to push an NDEF record to a remote device or tag. |args|
  // will contain one dictionary that contains the record data.
  void SubmitRecordForm(const base::ListValue* args);
  void OnSubmitRecordFormFailed(const std::string& error_message);

  // Callback passed to NfcAdapterFactory::GetAdapter.
  void OnGetAdapter(scoped_refptr<NfcAdapter> adapter);

  // Stores the properties of the NFC adapter in |out|, mapping these to keys
  // that will be read by JS.
  void GetAdapterProperties(base::DictionaryValue* out);

  // Stores the properties of the NFC peer in |out|, mapping these to keys
  // that will be read by JS. |out| will not be modified, if no peer is known.
  void GetPeerProperties(base::DictionaryValue* out);

  // Stores the properties of the NFC tag in |out|, mapping these to keys
  // that will be read by JS. |out| will not be modified, if no tag is known.
  void GetTagProperties(base::DictionaryValue* out);

  // Returns the records in |message| by populating |out|, in which
  // they have been converted to a JS friendly format.
  void GetRecordList(const NfcNdefMessage& message, base::ListValue* out);

  // Updates the data displayed in the UI for the current adapter.
  void UpdateAdapterInfo();

  // Updates the data displayed in the UI for the current peer.
  void UpdatePeerInfo();

  // Updates the data displayed in the UI for the current tag.
  void UpdateTagInfo();

  // The NfcAdapter object.
  scoped_refptr<NfcAdapter> nfc_adapter_;

  // The cached identifier of the most recent NFC peer device found.
  std::string peer_identifier_;

  // The cached identifier of the most recent NFC tag found.
  std::string tag_identifier_;

  DISALLOW_COPY_AND_ASSIGN(NfcDebugMessageHandler);
};

NfcDebugMessageHandler::NfcDebugMessageHandler() {
}

NfcDebugMessageHandler::~NfcDebugMessageHandler() {
  if (!nfc_adapter_.get())
    return;
  nfc_adapter_->RemoveObserver(this);
  if (!peer_identifier_.empty()) {
    NfcPeer* peer = nfc_adapter_->GetPeer(peer_identifier_);
    if (peer)
      peer->RemoveObserver(this);
  }
  if (!tag_identifier_.empty()) {
    NfcTag* tag = nfc_adapter_->GetTag(tag_identifier_);
    if (tag)
      tag->RemoveObserver(this);
  }
}

void NfcDebugMessageHandler::AdapterPresentChanged(
    NfcAdapter* adapter,
    bool present) {
  UpdateAdapterInfo();
}

void NfcDebugMessageHandler::AdapterPoweredChanged(
    NfcAdapter* adapter,
    bool powered) {
  UpdateAdapterInfo();
}

void NfcDebugMessageHandler::AdapterPollingChanged(
    NfcAdapter* adapter,
    bool polling) {
  UpdateAdapterInfo();
}

void NfcDebugMessageHandler::TagFound(NfcAdapter* adapter, NfcTag* tag) {
  VLOG(1) << "Found NFC tag: " << tag->GetIdentifier();
  tag->AddObserver(this);
  tag_identifier_ = tag->GetIdentifier();
  tag->GetNdefTagTechnology()->AddObserver(this);
  UpdateAdapterInfo();
  UpdateTagInfo();
}

void NfcDebugMessageHandler::TagLost(NfcAdapter*adapter, NfcTag* tag) {
  VLOG(1) << "Lost NFC tag: " << tag->GetIdentifier();
  tag->RemoveObserver(this);
  tag->GetNdefTagTechnology()->RemoveObserver(this);
  tag_identifier_.clear();
  UpdateAdapterInfo();
  UpdateTagInfo();
}

void NfcDebugMessageHandler::PeerFound(NfcAdapter* adaper, NfcPeer* peer) {
  VLOG(1) << "Found NFC peer device: " << peer->GetIdentifier();
  peer->AddObserver(this);
  peer_identifier_ = peer->GetIdentifier();
  UpdateAdapterInfo();
  UpdatePeerInfo();
}

void NfcDebugMessageHandler::PeerLost(NfcAdapter* adapter, NfcPeer* peer) {
  VLOG(1) << "Lost NFC peer device: " << peer->GetIdentifier();
  peer->RemoveObserver(this);
  peer_identifier_.clear();
  UpdateAdapterInfo();
  UpdatePeerInfo();
}

void NfcDebugMessageHandler::RecordReceived(
      NfcTag* tag,
      const NfcNdefRecord* record) {
  if (tag->GetIdentifier() != tag_identifier_) {
    LOG(WARNING) << "Records received from unknown tag: "
                 << tag->GetIdentifier();
    return;
  }
  UpdateTagInfo();
}

void NfcDebugMessageHandler::RecordReceived(
      NfcPeer* peer,
      const NfcNdefRecord* record) {
  if (peer->GetIdentifier() != peer_identifier_) {
    LOG(WARNING) << "Records received from unknown peer: "
                 << peer->GetIdentifier();
    return;
  }
  UpdatePeerInfo();
}

void NfcDebugMessageHandler::TagReady(NfcTag* tag) {
  if (tag_identifier_ != tag->GetIdentifier()) {
    LOG(WARNING) << "Unknown tag became ready: " << tag->GetIdentifier();
    return;
  }
  VLOG(1) << "Tag ready: " << tag->GetIdentifier();
  UpdateTagInfo();
}

void NfcDebugMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kInitializeCallback,
      base::Bind(&NfcDebugMessageHandler::Initialize,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kSetAdapterPowerCallback,
      base::Bind(&NfcDebugMessageHandler::SetAdapterPower,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kSetAdapterPollingCallback,
      base::Bind(&NfcDebugMessageHandler::SetAdapterPolling,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kSubmitRecordFormCallback,
      base::Bind(&NfcDebugMessageHandler::SubmitRecordForm,
                 base::Unretained(this)));
}

void NfcDebugMessageHandler::Initialize(const base::ListValue* args) {
  bool nfc_available = NfcAdapterFactory::IsNfcAvailable();
  base::FundamentalValue available(nfc_available);
  web_ui()->CallJavascriptFunction(kOnNfcAvailabilityDeterminedFunction,
                                   available);
  if (!nfc_available) {
    LOG(WARNING) << "NFC is not available on current platform.";
    return;
  }
  NfcAdapterFactory::GetAdapter(
      base::Bind(&NfcDebugMessageHandler::OnGetAdapter,
                 base::Unretained(this)));
}

void NfcDebugMessageHandler::SetAdapterPower(const base::ListValue* args) {
  DCHECK(1 == args->GetSize());
  DCHECK(nfc_adapter_.get());
  bool powered;
  args->GetBoolean(0, &powered);
  VLOG(1) << "Setting adapter power: " << powered;
  nfc_adapter_->SetPowered(
      powered, base::Bind(&base::DoNothing),
      base::Bind(&NfcDebugMessageHandler::OnSetAdapterPowerError,
                 base::Unretained(this)));
}

void NfcDebugMessageHandler::OnSetAdapterPowerError() {
  LOG(ERROR) << "Failed to set NFC adapter power.";
  web_ui()->CallJavascriptFunction(kOnSetAdapterPowerFailedFunction);
}

void NfcDebugMessageHandler::SetAdapterPolling(const base::ListValue* args) {
  DCHECK(1 == args->GetSize());
  DCHECK(nfc_adapter_.get());
  bool start = false;
  bool result = args->GetBoolean(0, &start);
  DCHECK(result);
  if (start) {
    VLOG(1) << "Starting NFC poll loop.";
    nfc_adapter_->StartPolling(
        base::Bind(&base::DoNothing),
        base::Bind(&NfcDebugMessageHandler::OnSetAdapterPollingError,
                   base::Unretained(this)));
  } else {
    VLOG(1) << "Stopping NFC poll loop.";
    nfc_adapter_->StopPolling(
        base::Bind(&base::DoNothing),
        base::Bind(&NfcDebugMessageHandler::OnSetAdapterPollingError,
                   base::Unretained(this)));
  }
}

void NfcDebugMessageHandler::OnSetAdapterPollingError() {
  LOG(ERROR) << "Failed to start/stop polling.";
  web_ui()->CallJavascriptFunction(kOnSetAdapterPollingFailedFunction);
}

void NfcDebugMessageHandler::SubmitRecordForm(const base::ListValue* args) {
  DCHECK(1 == args->GetSize());
  DCHECK(nfc_adapter_.get());
  const base::DictionaryValue* record_data_const = NULL;
  if (!args->GetDictionary(0, &record_data_const)) {
    NOTREACHED();
    return;
  }

  if (peer_identifier_.empty() && tag_identifier_.empty()) {
    OnSubmitRecordFormFailed("No peer or tag present.");
    return;
  }

  std::string type;
  if (!record_data_const->GetString(kRecordTypeProperty, &type)) {
    OnSubmitRecordFormFailed("Record type not provided.");
    return;
  }

  base::DictionaryValue* record_data = record_data_const->DeepCopy();
  record_data->Remove(kRecordTypeProperty, NULL);

  // Convert the "targetSize" field to a double, in case JS stored it as an
  // integer.
  int target_size;
  if (record_data->GetInteger(NfcNdefRecord::kFieldTargetSize, &target_size)) {
    record_data->SetDouble(NfcNdefRecord::kFieldTargetSize,
                           static_cast<double>(target_size));
  }

  NfcNdefRecord record;
  if (!record.Populate(RecordTypeStringValueToEnum(type), record_data)) {
    OnSubmitRecordFormFailed("Invalid record data provided. Missing required "
                             "fields?");
    return;
  }

  if (!peer_identifier_.empty()) {
    NfcPeer* peer = nfc_adapter_->GetPeer(peer_identifier_);
    if (!peer) {
      OnSubmitRecordFormFailed("The current NFC adapter doesn't seem to know "
                               "about peer: " + peer_identifier_);
      return;
    }
    NfcNdefMessage message;
    message.AddRecord(&record);
    peer->PushNdef(message,
                   base::Bind(&base::DoNothing),
                   base::Bind(&NfcDebugMessageHandler::OnSubmitRecordFormFailed,
                              base::Unretained(this),
                              "Failed to push NDEF record."));
    return;
  }
  NfcTag* tag = nfc_adapter_->GetTag(tag_identifier_);
  if (!tag) {
    OnSubmitRecordFormFailed("The current NFC tag doesn't seem to known about "
                             "tag: " + tag_identifier_);
    return;
  }
  NfcNdefMessage message;
  message.AddRecord(&record);
  tag->GetNdefTagTechnology()->WriteNdef(
                 message,
                 base::Bind(&base::DoNothing),
                 base::Bind(&NfcDebugMessageHandler::OnSubmitRecordFormFailed,
                            base::Unretained(this),
                            "Failed to write NDEF record."));
}

void NfcDebugMessageHandler::OnSubmitRecordFormFailed(
    const std::string& error_message) {
  LOG(ERROR) << "SubmitRecordForm failed: " << error_message;
  web_ui()->CallJavascriptFunction(kOnSubmitRecordFormFailedFunction,
                                   base::StringValue(error_message));
}

void NfcDebugMessageHandler::OnGetAdapter(
    scoped_refptr<NfcAdapter> adapter) {
  if (nfc_adapter_.get())
    return;
  nfc_adapter_ = adapter;
  nfc_adapter_->AddObserver(this);
  UpdateAdapterInfo();

  NfcAdapter::PeerList peers;
  nfc_adapter_->GetPeers(&peers);
  for (NfcAdapter::PeerList::const_iterator iter = peers.begin();
       iter != peers.end(); ++iter) {
    PeerFound(nfc_adapter_.get(), *iter);
  }

  NfcAdapter::TagList tags;
  nfc_adapter_->GetTags(&tags);
  for (NfcAdapter::TagList::const_iterator iter = tags.begin();
       iter != tags.end(); ++iter) {
    TagFound(nfc_adapter_.get(), *iter);
  }
}

void NfcDebugMessageHandler::GetAdapterProperties(
    base::DictionaryValue* out) {
  if (!nfc_adapter_.get()) {
    VLOG(1) << "NFC adapter hasn't been received yet.";
    return;
  }
  out->SetBoolean(kAdapterPresentProperty, nfc_adapter_->IsPresent());
  out->SetBoolean(kAdapterPollingProperty, nfc_adapter_->IsPolling());
  out->SetBoolean(kAdapterPoweredProperty, nfc_adapter_->IsPowered());

  NfcAdapter::PeerList peers;
  nfc_adapter_->GetPeers(&peers);
  out->SetInteger(kAdapterPeersProperty, static_cast<int>(peers.size()));

  NfcAdapter::TagList tags;
  nfc_adapter_->GetTags(&tags);
  out->SetInteger(kAdapterTagsProperty, static_cast<int>(tags.size()));
}

void NfcDebugMessageHandler::GetPeerProperties(base::DictionaryValue* out) {
  if (peer_identifier_.empty()) {
    VLOG(1) << "No known peer exists.";
    return;
  }
  if (!nfc_adapter_.get()) {
    VLOG(1) << "NFC adapter hasn't been received yet.";
    return;
  }
  NfcPeer* peer = nfc_adapter_->GetPeer(peer_identifier_);
  if (!peer) {
    LOG(ERROR) << "The current NFC adapter doesn't seem to know about peer: "
               << peer_identifier_;
    return;
  }
  out->SetString(kPeerIdentifierProperty, peer_identifier_);

  base::ListValue* records = new base::ListValue();
  GetRecordList(peer->GetNdefMessage(), records);
  out->Set(kPeerRecordsProperty, records);
}

void NfcDebugMessageHandler::GetTagProperties(base::DictionaryValue* out) {
  if (tag_identifier_.empty()) {
    VLOG(1) << "No known tag exists.";
    return;
  }
  if (!nfc_adapter_.get()) {
    VLOG(1) << "NFC adapter hasn't been received yet.";
    return;
  }
  NfcTag* tag = nfc_adapter_->GetTag(tag_identifier_);
  if (!tag) {
    LOG(ERROR) << "The current NFC adapter doesn't seem to know about tag: "
               << tag_identifier_;
    return;
  }
  out->SetString(kTagIdentifierProperty, tag_identifier_);
  out->SetString(kTagTypeProperty, TagTypeToString(tag->GetType()));
  out->SetBoolean(kTagReadOnlyProperty, tag->IsReadOnly());
  out->SetString(kTagSupportedProtocolProperty,
                 TagProtocolToString(tag->GetSupportedProtocol()));

  base::ListValue* technologies = new base::ListValue();
  NfcTagTechnology::TechnologyTypeMask technology_mask =
      tag->GetSupportedTechnologies();
  if (technology_mask & NfcTagTechnology::kTechnologyTypeNfcA)
    technologies->AppendString(kTagTechnologyNfcA);
  if (technology_mask & NfcTagTechnology::kTechnologyTypeNfcB)
    technologies->AppendString(kTagTechnologyNfcB);
  if (technology_mask & NfcTagTechnology::kTechnologyTypeNfcF)
    technologies->AppendString(kTagTechnologyNfcF);
  if (technology_mask & NfcTagTechnology::kTechnologyTypeNfcV)
    technologies->AppendString(kTagTechnologyNfcV);
  if (technology_mask & NfcTagTechnology::kTechnologyTypeIsoDep)
    technologies->AppendString(kTagTechnologyIsoDep);
  if (technology_mask & NfcTagTechnology::kTechnologyTypeNdef)
    technologies->AppendString(kTagTechnologyNdef);
  out->Set(kTagSupportedTechnologiesProperty, technologies);

  base::ListValue* records = new base::ListValue();
  GetRecordList(tag->GetNdefTagTechnology()->GetNdefMessage(), records);
  out->Set(kTagRecordsProperty, records);
}

void NfcDebugMessageHandler::GetRecordList(const NfcNdefMessage& message,
                                           base::ListValue* out) {
  for (NfcNdefMessage::RecordList::const_iterator iter =
          message.records().begin();
      iter != message.records().end(); ++iter) {
    const NfcNdefRecord* record = (*iter);
    base::DictionaryValue* record_data = record->data().DeepCopy();
    record_data->SetString(kRecordTypeProperty,
                           RecordTypeToString(record->type()));
    out->Append(record_data);
  }
}

void NfcDebugMessageHandler::UpdateAdapterInfo() {
  base::DictionaryValue data;
  GetAdapterProperties(&data);
  web_ui()->CallJavascriptFunction(kOnNfcAdapterInfoChangedFunction, data);
}

void NfcDebugMessageHandler::UpdatePeerInfo() {
  base::DictionaryValue data;
  GetPeerProperties(&data);
  web_ui()->CallJavascriptFunction(kOnNfcPeerDeviceInfoChangedFunction, data);
}

void NfcDebugMessageHandler::UpdateTagInfo() {
  base::DictionaryValue data;
  GetTagProperties(&data);
  web_ui()->CallJavascriptFunction(kOnNfcTagInfoChangedFunction, data);
}

}  // namespace

NfcDebugUI::NfcDebugUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(new NfcDebugMessageHandler());

  content::WebUIDataSource* html_source =
    content::WebUIDataSource::Create(chrome::kChromeUINfcDebugHost);
  html_source->SetUseJsonJSFormatV2();

  html_source->AddLocalizedString("titleText", IDS_NFC_DEBUG_TITLE);
  html_source->AddLocalizedString("notSupportedText",
                                  IDS_NFC_DEBUG_NOT_SUPPORTED);
  html_source->AddLocalizedString("adapterHeaderText",
                                  IDS_NFC_DEBUG_ADAPTER_HEADER);
  html_source->AddLocalizedString("adapterPowerOnText",
                                  IDS_NFC_DEBUG_ADAPTER_POWER_ON);
  html_source->AddLocalizedString("adapterPowerOffText",
                                  IDS_NFC_DEBUG_ADAPTER_POWER_OFF);
  html_source->AddLocalizedString("adapterStartPollText",
                                  IDS_NFC_DEBUG_ADAPTER_START_POLL);
  html_source->AddLocalizedString("adapterStopPollText",
                                  IDS_NFC_DEBUG_ADAPTER_STOP_POLL);
  html_source->AddLocalizedString("ndefFormHeaderText",
                                  IDS_NFC_DEBUG_NDEF_FORM_HEADER);
  html_source->AddLocalizedString("ndefFormTypeTextText",
                                  IDS_NFC_DEBUG_NDEF_FORM_TYPE_TEXT);
  html_source->AddLocalizedString("ndefFormTypeUriText",
                                  IDS_NFC_DEBUG_NDEF_FORM_TYPE_URI);
  html_source->AddLocalizedString("ndefFormTypeSmartPosterText",
                                  IDS_NFC_DEBUG_NDEF_FORM_TYPE_SMART_POSTER);
  html_source->AddLocalizedString("ndefFormWriteButtonText",
                                  IDS_NFC_DEBUG_NDEF_FORM_WRITE_BUTTON);
  html_source->AddLocalizedString("ndefFormFieldTextText",
                                  IDS_NFC_DEBUG_NDEF_FORM_FIELD_TEXT);
  html_source->AddLocalizedString("ndefFormFieldEncodingText",
                                  IDS_NFC_DEBUG_NDEF_FORM_FIELD_ENCODING);
  html_source->AddLocalizedString("ndefFormFieldLanguageCodeText",
                                  IDS_NFC_DEBUG_NDEF_FORM_FIELD_LANGUAGE_CODE);
  html_source->AddLocalizedString("ndefFormFieldUriText",
                                  IDS_NFC_DEBUG_NDEF_FORM_FIELD_URI);
  html_source->AddLocalizedString("ndefFormFieldMimeTypeText",
                                  IDS_NFC_DEBUG_NDEF_FORM_FIELD_MIME_TYPE);
  html_source->AddLocalizedString("ndefFormFieldTargetSizeText",
                                  IDS_NFC_DEBUG_NDEF_FORM_FIELD_TARGET_SIZE);
  html_source->AddLocalizedString("ndefFormFieldTitleTextText",
                                  IDS_NFC_DEBUG_NDEF_FORM_FIELD_TITLE_TEXT);
  html_source->AddLocalizedString("ndefFormFieldTitleEncodingText",
                                  IDS_NFC_DEBUG_NDEF_FORM_FIELD_TITLE_ENCODING);
  html_source->AddLocalizedString(
      "ndefFormFieldTitleLanguageCodeText",
      IDS_NFC_DEBUG_NDEF_FORM_FIELD_TITLE_LANGUAGE_CODE);
  html_source->AddLocalizedString("ndefFormPushButtonText",
                                  IDS_NFC_DEBUG_NDEF_FORM_PUSH_BUTTON);
  html_source->AddLocalizedString("nfcPeerHeaderText",
                                  IDS_NFC_DEBUG_NFC_PEER_HEADER);
  html_source->AddLocalizedString("nfcTagHeaderText",
                                  IDS_NFC_DEBUG_NFC_TAG_HEADER);
  html_source->AddLocalizedString("recordsHeaderText",
                                  IDS_NFC_DEBUG_RECORDS_HEADER);
  html_source->AddLocalizedString("errorFailedToSetPowerText",
                                  IDS_NFC_DEBUG_ERROR_FAILED_TO_SET_POWER);
  html_source->AddLocalizedString("errorFailedToSetPollingText",
                                  IDS_NFC_DEBUG_ERROR_FAILED_TO_SET_POLLING);
  html_source->AddLocalizedString("errorFailedToSubmitPrefixText",
                                  IDS_NFC_DEBUG_ERROR_FAILED_TO_SUBMIT_PREFIX);
  html_source->SetJsonPath("strings.js");

  // Add required resources.
  html_source->AddResourcePath("nfc_debug.css", IDR_NFC_DEBUG_CSS);
  html_source->AddResourcePath("nfc_debug.js", IDR_NFC_DEBUG_JS);
  html_source->SetDefaultResource(IDR_NFC_DEBUG_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
}

NfcDebugUI::~NfcDebugUI() {
}

}  // namespace chromeos
