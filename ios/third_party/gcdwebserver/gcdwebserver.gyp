# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets' : [
    {
      'target_name' : 'gcdwebserver',
      'type': 'static_library',
      'include_dirs': [
        'src/GCDWebServer/Core',
        'src/GCDWebServer/Requests',
        'src/GCDWebServer/Responses',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src/GCDWebServer/Core',
          'src/GCDWebServer/Requests',
          'src/GCDWebServer/Responses',
        ],
      },
      'xcode_settings': {
        'CLANG_ENABLE_OBJC_ARC': 'YES',
      },
      'sources': [
        'src/GCDWebServer/Core/GCDWebServer.h',
        'src/GCDWebServer/Core/GCDWebServer.m',
        'src/GCDWebServer/Core/GCDWebServerConnection.h',
        'src/GCDWebServer/Core/GCDWebServerConnection.m',
        'src/GCDWebServer/Core/GCDWebServerFunctions.h',
        'src/GCDWebServer/Core/GCDWebServerFunctions.m',
        'src/GCDWebServer/Core/GCDWebServerHTTPStatusCodes.h',
        'src/GCDWebServer/Core/GCDWebServerPrivate.h',
        'src/GCDWebServer/Core/GCDWebServerRequest.h',
        'src/GCDWebServer/Core/GCDWebServerRequest.m',
        'src/GCDWebServer/Core/GCDWebServerResponse.h',
        'src/GCDWebServer/Core/GCDWebServerResponse.m',
        'src/GCDWebServer/Requests/GCDWebServerDataRequest.h',
        'src/GCDWebServer/Requests/GCDWebServerDataRequest.m',
        'src/GCDWebServer/Requests/GCDWebServerFileRequest.h',
        'src/GCDWebServer/Requests/GCDWebServerFileRequest.m',
        'src/GCDWebServer/Requests/GCDWebServerMultiPartFormRequest.h',
        'src/GCDWebServer/Requests/GCDWebServerMultiPartFormRequest.m',
        'src/GCDWebServer/Requests/GCDWebServerURLEncodedFormRequest.h',
        'src/GCDWebServer/Requests/GCDWebServerURLEncodedFormRequest.m',
        'src/GCDWebServer/Responses/GCDWebServerDataResponse.h',
        'src/GCDWebServer/Responses/GCDWebServerDataResponse.m',
        'src/GCDWebServer/Responses/GCDWebServerErrorResponse.h',
        'src/GCDWebServer/Responses/GCDWebServerErrorResponse.m',
        'src/GCDWebServer/Responses/GCDWebServerFileResponse.h',
        'src/GCDWebServer/Responses/GCDWebServerFileResponse.m',
        'src/GCDWebServer/Responses/GCDWebServerStreamedResponse.h',
        'src/GCDWebServer/Responses/GCDWebServerStreamedResponse.m',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/CFNetwork.framework',
          '$(SDKROOT)/System/Library/Frameworks/MobileCoreServices.framework',
          '$(SDKROOT)/usr/lib/libz.dylib',
        ],
      },
    },
  ],
}
