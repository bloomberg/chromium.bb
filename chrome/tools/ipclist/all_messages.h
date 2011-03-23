// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, hence no include guard.
// Inclusion of all message files present in the system.  Keep this file
// up-to-date when adding a new value to enum IPCMessageStart in
// ipc/ipc_message_utils.h to include the corresponding message file.
#include "chrome/browser/importer/importer_messages.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/common/nacl_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/safebrowsing_messages.h"
#include "chrome/common/service_messages.h"
#include "chrome/common/utility_messages.h"
#include "content/common/appcache_messages.h"
#include "content/common/child_process_messages.h"
#include "content/common/clipboard_messages.h"
#include "content/common/database_messages.h"
#include "content/common/desktop_notification_messages.h"
#include "content/common/device_orientation_messages.h"
#include "content/common/dom_storage_messages.h"
#include "content/common/drag_messages.h"
#include "content/common/file_system_messages.h"
#include "content/common/file_utilities_messages.h"
#include "content/common/geolocation_messages.h"
#include "content/common/gpu_messages.h"
#include "content/common/indexed_db_messages.h"
#include "content/common/mime_registry_messages.h"
#include "content/common/p2p_messages.h"
#include "content/common/pepper_file_messages.h"
#include "content/common/pepper_messages.h"
#include "content/common/plugin_messages.h"
#include "content/common/resource_messages.h"
#include "content/common/socket_stream_messages.h"
#include "content/common/speech_input_messages.h"
#include "content/common/view_messages.h"
#include "content/common/webblob_messages.h"
#include "content/common/worker_messages.h"
#include "ppapi/proxy/ppapi_messages.h"
