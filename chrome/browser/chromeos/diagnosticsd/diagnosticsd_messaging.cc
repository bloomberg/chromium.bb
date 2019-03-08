// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/diagnosticsd/diagnosticsd_messaging.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/diagnosticsd/diagnosticsd_bridge.h"
#include "chrome/browser/chromeos/diagnosticsd/mojo_utils.h"
#include "chrome/browser/extensions/api/messaging/native_message_port.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/services/diagnosticsd/public/mojom/diagnosticsd.mojom.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/system/handle.h"
#include "url/gurl.h"

class Profile;

namespace chromeos {

// Native application name that is used for passing UI messages between the
// diagnostics_processor daemon and extensions.
const char kDiagnosticsdUiMessageHost[] = "com.google.wilco_dtc";

// Error messages sent to the extension:
const char kDiagnosticsdUiMessageTooBigExtensionsError[] =
    "Message is too big.";
const char kDiagnosticsdUiExtraMessagesExtensionsError[] =
    "At most one message must be sent through the message channel.";

// Maximum allowed size of UI messages passed between the diagnostics_processor
// daemon and extensions.
const int kDiagnosticsdUiMessageMaxSize = 1000000;

namespace {

// List of extension IDs that will receive UI messages from the
// diagnostics_processor through the extensions native messaging system.
//
// Note: the list must be kept in sync with the allowed origins list in
// src/chrome/browser/extensions/api/messaging/native_message_host_chromeos.cc.
//
// TODO(crbug.com/907932,b/123926112): Populate the list once extension IDs are
// determined.
const char* const kAllowedExtensionIds[] = {};

// Extensions native message host implementation that is used when an
// extension requests a message channel to the diagnostics_processor daemon.
//
// The message is transmitted via the diagnosticsd daemon. One instance of this
// class allows only one message to be sent; at most one message will be sent in
// the reverse direction: it will contain the daemon's response.
class DiagnosticsdExtensionOwnedMessageHost final
    : public extensions::NativeMessageHost {
 public:
  DiagnosticsdExtensionOwnedMessageHost() = default;
  ~DiagnosticsdExtensionOwnedMessageHost() override = default;

  // extensions::NativeMessageHost:

  void Start(Client* client) override {
    DCHECK(!client_);
    client_ = client;
  }

  void OnMessage(const std::string& request_string) override {
    DCHECK(client_);

    if (is_disposed_) {
      // We already called CloseChannel() before so ignore messages arriving at
      // this point. This corner case can happen because CloseChannel() does its
      // job asynchronously.
      return;
    }

    if (message_from_extension_received_) {
      // Our implementation doesn't allow sending multiple messages from the
      // extension over the same instance.
      DisposeSelf(kDiagnosticsdUiExtraMessagesExtensionsError);
      return;
    }
    message_from_extension_received_ = true;

    if (request_string.size() > kDiagnosticsdUiMessageMaxSize) {
      DisposeSelf(kDiagnosticsdUiMessageTooBigExtensionsError);
      return;
    }

    DiagnosticsdBridge* const diagnosticsd_bridge = DiagnosticsdBridge::Get();
    if (!diagnosticsd_bridge) {
      VLOG(0) << "Cannot send message - no bridge to the daemon";
      DisposeSelf(kNotFoundError);
      return;
    }

    diagnosticsd::mojom::DiagnosticsdServiceProxy* const
        diagnosticsd_mojo_proxy =
            diagnosticsd_bridge->diagnosticsd_service_mojo_proxy();
    if (!diagnosticsd_mojo_proxy) {
      VLOG(0) << "Cannot send message - Mojo connection to the daemon isn't "
                 "bootstrapped yet";
      DisposeSelf(kNotFoundError);
      return;
    }

    mojo::ScopedHandle json_message_mojo_handle =
        CreateReadOnlySharedMemoryMojoHandle(request_string);
    if (!json_message_mojo_handle) {
      LOG(ERROR) << "Cannot create Mojo shared memory handle from string";
      DisposeSelf(kHostInputOutputError);
      return;
    }

    diagnosticsd_mojo_proxy->SendUiMessageToDiagnosticsProcessor(
        std::move(json_message_mojo_handle),
        base::BindOnce(&DiagnosticsdExtensionOwnedMessageHost::
                           OnResponseReceivedFromDaemon,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override {
    return task_runner_;
  }

 private:
  void DisposeSelf(const std::string& error_message) {
    DCHECK(!is_disposed_);
    is_disposed_ = true;
    client_->CloseChannel(error_message);
    // Prevent the Mojo call result, if it's still in flight, from being
    // forwarded to the extension.
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  void OnResponseReceivedFromDaemon(mojo::ScopedHandle response_json_message) {
    DCHECK(client_);
    DCHECK(!is_disposed_);

    if (!response_json_message) {
      // The call to the diagnostics_processor daemon failed or the daemon
      // provided no response, so just close the extension message channel as
      // it's intended to be used for one-time messages only.
      VLOG(1) << "Empty response, closing the extension message channel";
      DisposeSelf(std::string() /* error_message */);
      return;
    }

    std::unique_ptr<base::SharedMemory> response_json_shared_memory;
    base::StringPiece response_json_string = GetStringPieceFromMojoHandle(
        std::move(response_json_message), &response_json_shared_memory);
    if (response_json_string.empty()) {
      LOG(ERROR) << "Cannot read response from Mojo shared memory";
      DisposeSelf(kHostInputOutputError);
      return;
    }

    if (response_json_string.size() > kDiagnosticsdUiMessageMaxSize) {
      LOG(ERROR) << "The message received from the daemon is too big";
      DisposeSelf(kDiagnosticsdUiMessageTooBigExtensionsError);
      return;
    }

    if (response_json_string.size() > kDiagnosticsdUiMessageMaxSize) {
      LOG(ERROR) << "The message received from the daemon is too big";
      client_->CloseChannel(kHostInputOutputError);
      return;
    }

    client_->PostMessageFromNativeHost(response_json_string.as_string());
    DisposeSelf(std::string() /* error_message */);
  }

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_ =
      base::ThreadTaskRunnerHandle::Get();

  // Unowned.
  Client* client_ = nullptr;

  // Whether a message has already been received from the extension.
  bool message_from_extension_received_ = false;
  // Whether DisposeSelf() has already been called.
  bool is_disposed_ = false;

  // Must be the last member.
  base::WeakPtrFactory<DiagnosticsdExtensionOwnedMessageHost> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdExtensionOwnedMessageHost);
};

// Extensions native message host implementation that is used when the
// diagnostics_processor daemon sends (via the diagnosticsd daemon) a message to
// the extension.
//
// A new instance of this class should be created for each instance of the
// extension(s) that are allowed to receive messages from the
// diagnostics_processor daemon. Once the extension responds by posting a
// message back to this message channel, |send_response_callback| will be
// called.
class DiagnosticsdDaemonOwnedMessageHost final
    : public extensions::NativeMessageHost {
 public:
  DiagnosticsdDaemonOwnedMessageHost(
      const std::string& json_message_to_send,
      base::OnceCallback<void(const std::string& response)>
          send_response_callback)
      : json_message_to_send_(json_message_to_send),
        send_response_callback_(std::move(send_response_callback)) {
    DCHECK(send_response_callback_);
  }

  ~DiagnosticsdDaemonOwnedMessageHost() override {
    if (send_response_callback_) {
      // If no response was received from the extension, pass the empty result
      // to the callback to signal the error.
      std::move(send_response_callback_).Run(std::string() /* response */);
    }
  }

  // extensions::NativeMessageHost:

  void Start(Client* client) override {
    DCHECK(!client_);
    client_ = client;
    client_->PostMessageFromNativeHost(json_message_to_send_);
  }

  void OnMessage(const std::string& request_string) override {
    DCHECK(client_);
    if (!send_response_callback_) {
      // This happens when the extension sent more than one message via the
      // message channel, which is not supported in our case - therefore simply
      // discard these extra messages.
      return;
    }
    if (request_string.size() > kDiagnosticsdUiMessageMaxSize) {
      std::move(send_response_callback_).Run(std::string() /* response */);
      client_->CloseChannel(kDiagnosticsdUiMessageTooBigExtensionsError);
      return;
    }
    std::move(send_response_callback_).Run(request_string /* response */);
    client_->CloseChannel(std::string() /* error_message */);
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override {
    return task_runner_;
  }

 private:
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_ =
      base::ThreadTaskRunnerHandle::Get();
  const std::string json_message_to_send_;
  base::OnceCallback<void(const std::string& response)> send_response_callback_;
  // Unowned.
  Client* client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdDaemonOwnedMessageHost);
};

// Helper that wraps the specified OnceCallback and encapsulates logic that
// executes it once either of the following becomes true (whichever happens to
// be earlier):
// * Non-empty data was provided to this class via the ProcessResponse() method;
// * The ProcessResponse() method has been called the |wrapper_callback_count|
//   number of times.
class FirstNonEmptyMessageCallbackWrapper final {
 public:
  FirstNonEmptyMessageCallbackWrapper(
      base::OnceCallback<void(const std::string& response)> original_callback,
      int wrapper_callback_count)
      : original_callback_(std::move(original_callback)),
        pending_callback_count_(wrapper_callback_count) {
    DCHECK(original_callback_);
    DCHECK_GE(pending_callback_count_, 0);
    if (!pending_callback_count_)
      std::move(original_callback_).Run(std::string() /* response */);
  }

  ~FirstNonEmptyMessageCallbackWrapper() {
    if (original_callback_) {
      // Not all responses were received before this instance is destroyed, so
      // run the callback with an error result here.
      std::move(original_callback_).Run(std::string() /* response */);
    }
  }

  void ProcessResponse(const std::string& response) {
    if (!original_callback_) {
      // The response was already passed in one of the previous invocations.
      return;
    }
    if (!response.empty()) {
      std::move(original_callback_).Run(response);
      return;
    }
    --pending_callback_count_;
    DCHECK_GE(pending_callback_count_, 0);
    if (pending_callback_count_ == 0) {
      // This is the last response and all responses have been empty, so pass
      // the empty response.
      std::move(original_callback_).Run(std::string() /* response */);
      return;
    }
  }

 private:
  base::OnceCallback<void(const std::string& response)> original_callback_;
  int pending_callback_count_;

  DISALLOW_COPY_AND_ASSIGN(FirstNonEmptyMessageCallbackWrapper);
};

void DeliverMessageToExtension(
    Profile* profile,
    const std::string& extension_id,
    const std::string& json_message,
    base::OnceCallback<void(const std::string& response)>
        send_response_callback) {
  const extensions::PortId port_id(base::UnguessableToken::Create(),
                                   1 /* port_number */, true /* is_opener */);
  extensions::MessageService* const message_service =
      extensions::MessageService::Get(profile);
  auto native_message_host =
      std::make_unique<DiagnosticsdDaemonOwnedMessageHost>(
          json_message, std::move(send_response_callback));
  auto native_message_port = std::make_unique<extensions::NativeMessagePort>(
      message_service->GetChannelDelegate(), port_id,
      std::move(native_message_host));
  message_service->OpenChannelToExtension(
      extensions::ChannelEndpoint(profile), port_id,
      extensions::MessagingEndpoint::ForNativeApp(kDiagnosticsdUiMessageHost),
      std::move(native_message_port), extension_id, GURL(),
      std::string() /* channel_name */);
}

}  // namespace

std::unique_ptr<extensions::NativeMessageHost>
CreateExtensionOwnedDiagnosticsdMessageHost() {
  return std::make_unique<DiagnosticsdExtensionOwnedMessageHost>();
}

void DeliverDiagnosticsdUiMessageToExtensions(
    const std::string& json_message,
    base::OnceCallback<void(const std::string& response)>
        send_response_callback) {
  if (json_message.size() > kDiagnosticsdUiMessageMaxSize) {
    VLOG(1) << "Message received from the daemon is too big";
    return;
  }

  // Determine beforehand which extension instances should receive the event, in
  // order to be able to construct the wrapper callback with the needed counter.
  std::vector<std::pair<Profile*, std::string>> recipient_extensions;
  for (auto* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    for (const auto* extension_id : kAllowedExtensionIds) {
      if (extensions::ExtensionRegistry::Get(profile)
              ->enabled_extensions()
              .GetByID(extension_id)) {
        recipient_extensions.emplace_back(profile, extension_id);
      }
    }
  }

  // Build the wrapper callback in order to call |send_response_callback| once
  // when:
  // * either the first non-empty response is received from one of the
  //   extensions;
  // * or requests to all extensions completed with no response.
  base::RepeatingCallback<void(const std::string& response)>
      first_non_empty_message_forwarding_callback = base::BindRepeating(
          &FirstNonEmptyMessageCallbackWrapper::ProcessResponse,
          base::Owned(new FirstNonEmptyMessageCallbackWrapper(
              std::move(send_response_callback),
              static_cast<int>(recipient_extensions.size()))));

  for (const auto& profile_and_extension : recipient_extensions) {
    DeliverMessageToExtension(profile_and_extension.first,
                              profile_and_extension.second, json_message,
                              first_non_empty_message_forwarding_callback);
  }
}

}  // namespace chromeos
