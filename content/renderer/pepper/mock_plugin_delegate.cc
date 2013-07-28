// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/mock_plugin_delegate.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_delegate.h"
#include "content/renderer/pepper/plugin_module.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace content {

MockPluginDelegate::MockPluginDelegate() {
}

MockPluginDelegate::~MockPluginDelegate() {
}

void MockPluginDelegate::PluginFocusChanged(PepperPluginInstanceImpl* instance,
                                            bool focused) {
}

void MockPluginDelegate::PluginTextInputTypeChanged(
    PepperPluginInstanceImpl* instance) {
}

void MockPluginDelegate::PluginCaretPositionChanged(
    PepperPluginInstanceImpl* instance) {
}

void MockPluginDelegate::PluginRequestedCancelComposition(
    PepperPluginInstanceImpl* instance) {
}

void MockPluginDelegate::PluginSelectionChanged(
    PepperPluginInstanceImpl* instance) {
}

void MockPluginDelegate::SimulateImeSetComposition(
    const base::string16& text,
    const std::vector<WebKit::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
}

void MockPluginDelegate::SimulateImeConfirmComposition(
    const base::string16& text) {
}

void MockPluginDelegate::PluginCrashed(PepperPluginInstanceImpl* instance) {
}

void MockPluginDelegate::InstanceCreated(PepperPluginInstanceImpl* instance) {
}

void MockPluginDelegate::InstanceDeleted(PepperPluginInstanceImpl* instance) {
}

scoped_ptr< ::ppapi::thunk::ResourceCreationAPI>
    MockPluginDelegate::CreateResourceCreationAPI(
        PepperPluginInstanceImpl* instance) {
  return scoped_ptr< ::ppapi::thunk::ResourceCreationAPI>();
}

SkBitmap* MockPluginDelegate::GetSadPluginBitmap() {
  return NULL;
}

WebKit::WebPlugin* MockPluginDelegate::CreatePluginReplacement(
    const base::FilePath& file_path) {
  return NULL;
}

MockPluginDelegate::PlatformVideoDecoder*
MockPluginDelegate::CreateVideoDecoder(
    media::VideoDecodeAccelerator::Client* client,
    int32 command_buffer_route_id) {
  return NULL;
}

uint32_t MockPluginDelegate::GetAudioHardwareOutputSampleRate() {
  return 0;
}

uint32_t MockPluginDelegate::GetAudioHardwareOutputBufferSize() {
  return 0;
}

MockPluginDelegate::Broker* MockPluginDelegate::ConnectToBroker(
    PPB_Broker_Impl* client) {
  return NULL;
}

void MockPluginDelegate::NumberOfFindResultsChanged(int identifier,
                                                    int total,
                                                    bool final_result) {
}

void MockPluginDelegate::SelectedFindResultChanged(int identifier, int index) {
}

bool MockPluginDelegate::AsyncOpenFile(const base::FilePath& path,
                                       int flags,
                                       const AsyncOpenFileCallback& callback) {
  return false;
}

void MockPluginDelegate::AsyncOpenFileSystemURL(
    const GURL& path,
    int flags,
    const AsyncOpenFileSystemURLCallback& callback) {
}

bool MockPluginDelegate::IsFileSystemOpened(
    PP_Instance instance,
    PP_Resource resource) const {
  return false;
}

PP_FileSystemType MockPluginDelegate::GetFileSystemType(
    PP_Instance instance,
    PP_Resource resource) const {
  return PP_FILESYSTEMTYPE_INVALID;
}

GURL MockPluginDelegate::GetFileSystemRootUrl(
    PP_Instance instance,
    PP_Resource resource) const {
  return GURL();
}

void MockPluginDelegate::MakeDirectory(
    const GURL& path,
    bool recursive,
    const StatusCallback& callback) {
}

void MockPluginDelegate::Query(
    const GURL& path,
    const MetadataCallback& success_callback,
    const StatusCallback& error_callback) {
}

void MockPluginDelegate::ReadDirectoryEntries(
    const GURL& path,
    const ReadDirectoryCallback& success_callback,
    const StatusCallback& error_callback) {
}

void MockPluginDelegate::Touch(
    const GURL& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
}

void MockPluginDelegate::SetLength(
    const GURL& path,
    int64_t length,
    const StatusCallback& callback) {
}

void MockPluginDelegate::Delete(
    const GURL& path,
    const StatusCallback& callback) {
}

void MockPluginDelegate::Rename(
    const GURL& file_path,
    const GURL& new_file_path,
    const StatusCallback& callback) {
}

void MockPluginDelegate::ReadDirectory(
    const GURL& directory_path,
    const ReadDirectoryCallback& success_callback,
    const StatusCallback& error_callback) {
}

void MockPluginDelegate::SyncGetFileSystemPlatformPath(
    const GURL& url,
    base::FilePath* platform_path) {
  DCHECK(platform_path);
  *platform_path = base::FilePath();
}

scoped_refptr<base::MessageLoopProxy>
MockPluginDelegate::GetFileThreadMessageLoopProxy() {
  return scoped_refptr<base::MessageLoopProxy>();
}

uint32 MockPluginDelegate::TCPSocketCreate() {
  return 0;
}

void MockPluginDelegate::TCPSocketConnect(PPB_TCPSocket_Private_Impl* socket,
                                          uint32 socket_id,
                                          const std::string& host,
                                          uint16_t port) {
}

void MockPluginDelegate::TCPSocketConnectWithNetAddress(
    PPB_TCPSocket_Private_Impl* socket,
    uint32 socket_id,
    const PP_NetAddress_Private& addr) {
}

void MockPluginDelegate::TCPSocketSSLHandshake(
    uint32 socket_id,
    const std::string& server_name,
    uint16_t server_port,
    const std::vector<std::vector<char> >& trusted_certs,
    const std::vector<std::vector<char> >& untrusted_certs) {
}

void MockPluginDelegate::TCPSocketRead(uint32 socket_id,
                                       int32_t bytes_to_read) {
}

void MockPluginDelegate::TCPSocketWrite(uint32 socket_id,
                                        const std::string& buffer) {
}

void MockPluginDelegate::TCPSocketSetOption(
    uint32 socket_id,
    PP_TCPSocket_Option name,
    const ::ppapi::SocketOptionData& value) {
}

void MockPluginDelegate::TCPSocketDisconnect(uint32 socket_id) {
}

void MockPluginDelegate::RegisterTCPSocket(PPB_TCPSocket_Private_Impl* socket,
                                           uint32 socket_id) {
}

void MockPluginDelegate::TCPServerSocketListen(
    PP_Resource socket_resource,
    const PP_NetAddress_Private& addr,
    int32_t backlog) {
}

void MockPluginDelegate::TCPServerSocketAccept(uint32 server_socket_id) {
}

void MockPluginDelegate::TCPServerSocketStopListening(
    PP_Resource socket_resource,
    uint32 socket_id) {
}

bool MockPluginDelegate::X509CertificateParseDER(
    const std::vector<char>& der,
    ::ppapi::PPB_X509Certificate_Fields* fields) {
  return false;
}

FullscreenContainer* MockPluginDelegate::CreateFullscreenContainer(
    PepperPluginInstanceImpl* instance) {
  return NULL;
}

gfx::Size MockPluginDelegate::GetScreenSize() {
  return gfx::Size(1024, 768);
}

std::string MockPluginDelegate::GetDefaultEncoding() {
  return "iso-8859-1";
}

void MockPluginDelegate::ZoomLimitsChanged(double minimum_factor,
                                           double maximum_factor) {
}

base::SharedMemory* MockPluginDelegate::CreateAnonymousSharedMemory(
    size_t size) {
  return NULL;
}

::ppapi::Preferences MockPluginDelegate::GetPreferences() {
  return ::ppapi::Preferences();
}

bool MockPluginDelegate::LockMouse(PepperPluginInstanceImpl* instance) {
  return false;
}

void MockPluginDelegate::UnlockMouse(PepperPluginInstanceImpl* instance) {
}

bool MockPluginDelegate::IsMouseLocked(PepperPluginInstanceImpl* instance) {
  return false;
}

void MockPluginDelegate::DidChangeCursor(PepperPluginInstanceImpl* instance,
                                         const WebKit::WebCursorInfo& cursor) {
}

void MockPluginDelegate::DidReceiveMouseEvent(
    PepperPluginInstanceImpl* instance) {
}

void MockPluginDelegate::SampleGamepads(WebKit::WebGamepads* data) {
  data->length = 0;
}

bool MockPluginDelegate::IsInFullscreenMode() {
  return false;
}

bool MockPluginDelegate::IsPageVisible() const {
  return true;
}

IPC::PlatformFileForTransit MockPluginDelegate::ShareHandleWithRemote(
      base::PlatformFile handle,
      base::ProcessId target_process_id,
      bool should_close_source) const {
  return IPC::InvalidPlatformFileForTransit();
}

bool MockPluginDelegate::IsRunningInProcess(PP_Instance instance) const {
  return false;
}

void MockPluginDelegate::HandleDocumentLoad(
    PepperPluginInstanceImpl* instance,
    const WebKit::WebURLResponse& response) {
}

RendererPpapiHost* MockPluginDelegate::CreateExternalPluginModule(
    scoped_refptr<PluginModule> module,
    const base::FilePath& path,
    ::ppapi::PpapiPermissions permissions,
    const IPC::ChannelHandle& channel_handle,
    base::ProcessId plugin_pid,
    int plugin_child_id) {
  return NULL;
}

}  // namespace content
