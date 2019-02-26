// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/diagnosticsd/diagnosticsd_messaging.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/diagnosticsd/diagnosticsd_bridge.h"
#include "chrome/browser/chromeos/diagnosticsd/mojo_utils.h"
#include "chrome/services/diagnosticsd/public/mojom/diagnosticsd.mojom.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "mojo/public/cpp/system/handle.h"

namespace chromeos {

// Native application name that is used for passing UI messages between the
// diagnostics_processor daemon and extensions.
const char kDiagnosticsdUiMessageHost[] = "com.google.wilco_dtc";

// Error nessages sent to the extension:
const char kDiagnosticsdUiMessageTooBigExtensionsError[] =
    "Message is too big.";
const char kDiagnosticsdUiExtraMessagesExtensionsError[] =
    "At most one message must be sent through the message channel.";

// Maximum allowed size of UI messages passed between the diagnostics_processor
// daemon and extensions.
const int kDiagnosticsdUiMessageMaxSize = 1000000;

namespace {

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

}  // namespace

std::unique_ptr<extensions::NativeMessageHost>
CreateExtensionOwnedDiagnosticsdMessageHost() {
  return std::make_unique<DiagnosticsdExtensionOwnedMessageHost>();
}

}  // namespace chromeos
