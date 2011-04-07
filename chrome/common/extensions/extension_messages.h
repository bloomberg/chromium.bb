// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for extensions.
// Multiply-included message file, hence no include guard.

#include "base/shared_memory.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_extent.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/web_apps.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START ExtensionMsgStart

// Parameters structure for ExtensionHostMsg_Request.
IPC_STRUCT_BEGIN(ExtensionHostMsg_DomMessage_Params)
  // Message name.
  IPC_STRUCT_MEMBER(std::string, name)

  // List of message arguments.
  IPC_STRUCT_MEMBER(ListValue, arguments)

  // URL of the frame request was sent from.
  IPC_STRUCT_MEMBER(GURL, source_url)

  // Unique request id to match requests and responses.
  IPC_STRUCT_MEMBER(int, request_id)

  // True if request has a callback specified.
  IPC_STRUCT_MEMBER(bool, has_callback)

  // True if request is executed in response to an explicit user gesture.
  IPC_STRUCT_MEMBER(bool, user_gesture)
IPC_STRUCT_END()

// Allows an extension to execute code in a tab.
IPC_STRUCT_BEGIN(ExtensionMsg_ExecuteCode_Params)
  // The extension API request id, for responding.
  IPC_STRUCT_MEMBER(int, request_id)

  // The ID of the requesting extension. To know which isolated world to
  // execute the code inside of.
  IPC_STRUCT_MEMBER(std::string, extension_id)

  // Whether the code is JavaScript or CSS.
  IPC_STRUCT_MEMBER(bool, is_javascript)

  // String of code to execute.
  IPC_STRUCT_MEMBER(std::string, code)

  // Whether to inject into all frames, or only the root frame.
  IPC_STRUCT_MEMBER(bool, all_frames)

  // Whether to execute code in the main world (as opposed to an isolated
  // world).
  IPC_STRUCT_MEMBER(bool, in_main_world)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(WebApplicationInfo::IconInfo)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
  IPC_STRUCT_TRAITS_MEMBER(data)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebApplicationInfo)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(description)
  IPC_STRUCT_TRAITS_MEMBER(app_url)
  IPC_STRUCT_TRAITS_MEMBER(icons)
  IPC_STRUCT_TRAITS_MEMBER(permissions)
  IPC_STRUCT_TRAITS_MEMBER(launch_container)
IPC_STRUCT_TRAITS_END()

// Singly-included section for custom IPC traits.
#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_MESSAGES_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_MESSAGES_H_

// IPC_MESSAGE macros choke on extra , in the std::map, when expanding. We need
// to typedef it to avoid that.
// Substitution map for l10n messages.
typedef std::map<std::string, std::string> SubstitutionMap;

struct ExtensionMsg_Loaded_Params {
  ExtensionMsg_Loaded_Params();
  ~ExtensionMsg_Loaded_Params();
  explicit ExtensionMsg_Loaded_Params(const Extension* extension);

  // A copy constructor is needed because this structure can end up getting
  // copied inside the IPC machinery on gcc <= 4.2.
  ExtensionMsg_Loaded_Params(const ExtensionMsg_Loaded_Params& other);

  // Creates a new extension from the data in this object.
  scoped_refptr<Extension> ConvertToExtension() const;

  // The subset of the extension manifest data we send to renderers.
  scoped_ptr<DictionaryValue> manifest;

  // The location the extension was installed from.
  Extension::Location location;

  // The path the extension was loaded from. This is used in the renderer only
  // to generate the extension ID for extensions that are loaded unpacked.
  FilePath path;

  // We keep this separate so that it can be used in logging.
  std::string id;
};

namespace IPC {

template <>
struct ParamTraits<URLPattern> {
  typedef URLPattern param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ExtensionExtent> {
  typedef ExtensionExtent param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ExtensionMsg_Loaded_Params> {
  typedef ExtensionMsg_Loaded_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_MESSAGES_H_

// Messages sent from the browser to the renderer.

// The browser sends this message in response to all extension api calls.
IPC_MESSAGE_ROUTED4(ExtensionMsg_Response,
                    int /* request_id */,
                    bool /* success */,
                    std::string /* response */,
                    std::string /* error */)

// This message is optionally routed.  If used as a control message, it
// will call a javascript function in every registered context in the
// target process.  If routed, it will be restricted to the contexts that
// are part of the target RenderView.
// If |extension_id| is non-empty, the function will be invoked only in
// contexts owned by the extension. |args| is a list of primitive Value types
// that are passed to the function.
IPC_MESSAGE_ROUTED4(ExtensionMsg_MessageInvoke,
                    std::string /* extension_id */,
                    std::string /* function_name */,
                    ListValue /* args */,
                    GURL /* event URL */)

// Tell the renderer process all known extension function names.
IPC_MESSAGE_CONTROL1(ExtensionMsg_SetFunctionNames,
                     std::vector<std::string>)

// TODO(aa): SetAPIPermissions, SetHostPermissions, and possibly
// UpdatePageActions should be replaced with just sending additional data in
// ExtensionLoaded. See: crbug.com/70516.

// Tell the renderer process which permissions the given extension has. See
// Extension::Permissions for which elements correspond to which permissions.
IPC_MESSAGE_CONTROL2(ExtensionMsg_SetAPIPermissions,
                     std::string /* extension_id */,
                     std::set<std::string> /* permissions */)

// Tell the renderer process which host permissions the given extension has.
IPC_MESSAGE_CONTROL2(ExtensionMsg_SetHostPermissions,
                     GURL /* source extension's origin */,
                     /* URLPatterns the extension can access */
                     std::vector<URLPattern>)

// Tell the renderer process all known page action ids for a particular
// extension.
IPC_MESSAGE_CONTROL2(ExtensionMsg_UpdatePageActions,
                     std::string /* extension_id */,
                     std::vector<std::string> /* page_action_ids */)

// Notifies the renderer that an extension was loaded in the browser.
IPC_MESSAGE_CONTROL1(ExtensionMsg_Loaded,
                     ExtensionMsg_Loaded_Params)

// Notifies the renderer that an extension was unloaded in the browser.
IPC_MESSAGE_CONTROL1(ExtensionMsg_Unloaded,
                     std::string)

// Updates the scripting whitelist for extensions in the render process. This is
// only used for testing.
IPC_MESSAGE_CONTROL1(ExtensionMsg_SetScriptingWhitelist,
                     Extension::ScriptingWhitelist /* extenison ids */)

// Notification that renderer should run some JavaScript code.
IPC_MESSAGE_ROUTED1(ExtensionMsg_ExecuteCode,
                    ExtensionMsg_ExecuteCode_Params)

// Notification that the user scripts have been updated. It has one
// SharedMemoryHandle argument consisting of the pickled script data. This
// handle is valid in the context of the renderer.
IPC_MESSAGE_CONTROL1(ExtensionMsg_UpdateUserScripts,
                     base::SharedMemoryHandle)

// Requests application info for the page. The renderer responds back with
// ExtensionHostMsg_DidGetApplicationInfo.
IPC_MESSAGE_ROUTED1(ExtensionMsg_GetApplicationInfo,
                    int32 /*page_id*/)

// Messages sent from the renderer to the browser.

// A renderer sends this message when an extension process starts an API
// request. The browser will always respond with a ExtensionMsg_Response.
IPC_MESSAGE_ROUTED1(ExtensionHostMsg_Request,
                    ExtensionHostMsg_DomMessage_Params)

// Notify the browser that the given extension added a listener to an event.
IPC_MESSAGE_CONTROL2(ExtensionHostMsg_AddListener,
                     std::string /* extension_id */,
                     std::string /* name */)

// Notify the browser that the given extension removed a listener from an
// event.
IPC_MESSAGE_CONTROL2(ExtensionHostMsg_RemoveListener,
                     std::string /* extension_id */,
                     std::string /* name */)

// Open a channel to all listening contexts owned by the extension with
// the given ID.  This always returns a valid port ID which can be used for
// sending messages.  If an error occurred, the opener will be notified
// asynchronously.
IPC_SYNC_MESSAGE_CONTROL4_1(ExtensionHostMsg_OpenChannelToExtension,
                            int /* routing_id */,
                            std::string /* source_extension_id */,
                            std::string /* target_extension_id */,
                            std::string /* channel_name */,
                            int /* port_id */)

// Get a port handle to the given tab.  The handle can be used for sending
// messages to the extension.
IPC_SYNC_MESSAGE_CONTROL4_1(ExtensionHostMsg_OpenChannelToTab,
                            int /* routing_id */,
                            int /* tab_id */,
                            std::string /* extension_id */,
                            std::string /* channel_name */,
                            int /* port_id */)

// Send a message to an extension process.  The handle is the value returned
// by ViewHostMsg_OpenChannelTo*.
IPC_MESSAGE_ROUTED2(ExtensionHostMsg_PostMessage,
                    int /* port_id */,
                    std::string /* message */)

// Send a message to an extension process.  The handle is the value returned
// by ViewHostMsg_OpenChannelTo*.
IPC_MESSAGE_CONTROL1(ExtensionHostMsg_CloseChannel,
                     int /* port_id */)

// Used to get the extension message bundle.
IPC_SYNC_MESSAGE_CONTROL1_1(ExtensionHostMsg_GetMessageBundle,
                            std::string /* extension id */,
                            SubstitutionMap /* message bundle */)

// Send from the renderer to the browser to return the script running result.
IPC_MESSAGE_ROUTED3(ExtensionHostMsg_ExecuteCodeFinished,
                    int /* request id */,
                    bool /* whether the script ran successfully */,
                    std::string /* error message */)

IPC_MESSAGE_ROUTED2(ExtensionHostMsg_DidGetApplicationInfo,
                    int32 /* page_id */,
                    WebApplicationInfo)

// Sent by the renderer to implement chrome.app.installApplication().
IPC_MESSAGE_ROUTED1(ExtensionHostMsg_InstallApplication,
                    WebApplicationInfo)
