// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEBUI_MOJO_FACADE_H_
#define IOS_WEB_WEBUI_MOJO_FACADE_H_

#include <map>
#include <memory>
#include <string>

#import "base/ios/weak_nsobject.h"
#include "mojo/public/cpp/system/watcher.h"

@protocol CRWJSInjectionEvaluator;

namespace base {
class DictionaryValue;
class Value;
}  // base

namespace service_manager {
namespace mojom {
class InterfaceProvider;
}  // mojom
}  // service_manager

namespace web {

// Facade class for Mojo. All inputs and outputs are optimized for communication
// with WebUI pages and hence use JSON format. Must be created used and
// destroyed on UI thread.
class MojoFacade {
 public:
  // Constructs MojoFacade. The calling code must retain the ownership of
  // |interface_provider| and |script_evaluator|, both can not be null.
  MojoFacade(service_manager::mojom::InterfaceProvider* interface_provider,
             id<CRWJSInjectionEvaluator> script_evaluator);
  ~MojoFacade();

  // Handles Mojo message received from WebUI page. Returns a valid JSON string
  // on success or empty string if supplied JSON does not have required
  // structure. Every message must have "name" and "args" keys, where "name" is
  // a string representing the name of Mojo message and "args" is a dictionary
  // with arguments specific for each message name.
  // Supported message names with their handler methods in parenthesis:
  //   interface_provider.getInterface (HandleInterfaceProviderGetInterface)
  //   core.close (HandleCoreClose)
  //   core.createMessagePipe (HandleCoreCreateMessagePipe)
  //   core.writeMessage (HandleCoreWriteMessage)
  //   core.readMessage (HandleCoreReadMessage)
  //   support.watch (HandleSupportWatch)
  //   support.cancelWatch (HandleSupportCancelWatch)
  std::string HandleMojoMessage(const std::string& mojo_message_as_json);

 private:
  // Extracts message name and arguments from the given JSON string obtained
  // from WebUI page. This method either succeeds or crashes the app (this
  // matches other platforms where Mojo API is strict on malformed input).
  void GetMessageNameAndArguments(
      const std::string& mojo_message_as_json,
      std::string* out_name,
      std::unique_ptr<base::DictionaryValue>* out_args);

  // Connects to specified Mojo interface. |args| is a dictionary which must
  // contain "interfaceName" key, which is a string representing a interface
  // name.
  // Returns MojoHandle as a number.
  std::unique_ptr<base::Value> HandleInterfaceProviderGetInterface(
      const base::DictionaryValue* args);

  // Closes the given handle. |args| is a dictionary which must contain "handle"
  // key, which is a number representing a MojoHandle.
  // Returns MojoResult as a number.
  std::unique_ptr<base::Value> HandleCoreClose(
      const base::DictionaryValue* args);

  // Creates a Mojo message pipe. |args| is a dictionary which must contain
  // "optionsDict" key. optionsDict is a dictionary with the following keys:
  //   - "struct_size" (a number representing the size of this struct; used to
  //     allow for future extensions);
  //   - "flags" (a number representing MojoCreateMessagePipeOptionsFlags; used
  //     to specify different modes of operation);
  // Returns a dictionary with "handle0" and "handle1" keys (the numbers
  // representing two ports for the message pipe).
  std::unique_ptr<base::Value> HandleCoreCreateMessagePipe(
      base::DictionaryValue* args);

  // Writes a message to the message pipe endpoint given by handle. |args| is a
  // dictionary which must contain the following keys:
  //   - "handle" (a number representing MojoHandle, the endpoint to write to);
  //   - "buffer" (a dictionary representing the message data; may be empty);
  //   - "handles" (an array representing any handles to attach; handles are
  //       transferred on success and will no longer be valid; may be empty);
  //   - "flags" (a number representing MojoWriteMessageFlags);
  // Returns MojoResult as a number.
  std::unique_ptr<base::Value> HandleCoreWriteMessage(
      base::DictionaryValue* args);

  // Reads a message from the message pipe endpoint given by handle. |args| is
  // a dictionary which must contain the following keys:
  //   - "handle" (a number representing MojoHandle, the endpoint to read from);
  //   - "flags" (a number representing MojoWriteMessageFlags);
  // Returns a dictionary with the following keys:
  //   - "result" (a number  representing MojoResult);
  //   - "buffer" (an array representing message data; non-empty only on
  //     success);
  //   - "handles" (an array representing MojoHandles transferred, if any);
  std::unique_ptr<base::Value> HandleCoreReadMessage(
      const base::DictionaryValue* args);

  // Begins watching a handle for signals to be satisfied or unsatisfiable.
  // |args| is a dictionary which must contain the following keys:
  //   - "handle" (a number representing a MojoHandle), "signals" (a number
  //     representing MojoHandleSignals to watch);
  //   - "callbackId" (a number representing the id which should be passed to
  //     __crWeb.mojo.mojoWatchSignal call);
  // Returns watch id as a number.
  std::unique_ptr<base::Value> HandleSupportWatch(
      const base::DictionaryValue* args);

  // Cancels a handle watch initiated by "support.watch". |args| is a dictionary
  // which must contain "watchId" key (a number representing id returned from
  // "support.watch").
  // Returns null.
  std::unique_ptr<base::Value> HandleSupportCancelWatch(
      const base::DictionaryValue* args);

  // Provides interfaces.
  service_manager::mojom::InterfaceProvider* interface_provider_;
  // Runs JavaScript on WebUI page.
  base::WeakNSProtocol<id<CRWJSInjectionEvaluator>> script_evaluator_;
  // Id of the last created watch.
  int last_watch_id_;
  // Currently active watches created through this facade.
  std::map<int, std::unique_ptr<mojo::Watcher>> watchers_;
};

}  // web

#endif  // IOS_WEB_WEBUI_MOJO_FACADE_H_
