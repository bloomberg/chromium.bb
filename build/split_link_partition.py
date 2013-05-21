# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This dict determines how chrome.dll is split into multiple parts.
{
  'parts': [
    # These sections are matched in order, and a matching input will go into
    # the part for the last block that matches. Inputs are lower()d before
    # the regex is run.

    # chrome.dll.
    [
      r'.*',
    ],

    # chrome1.dll.
    [
      r'\\libwebp\\.*\.lib$',
      r'\\media\\.*\.lib$',
      r'bindings',
      r'content_worker\.lib$',
      r'hunspell\.lib$',
      r'hyphen\.lib$',
      r'renderer\.lib$',
      r'v8.*\.lib$',
      r'webcore.*\.lib$',
      r'webkit.*\.lib$',
      r'webkit.*modules\.lib$',
      r'wtf\.lib$',
    ],
  ],

  # These go into all parts.
  'all': [
    # Duplicated code. Be sure that you really want N copies of your code
    # before adding it here. Hopefully we don't really need all of these.
    #r'_common\.lib$',  # TODO: This might be a bit general.
    #r'\\base\\base.*\.lib$',
    #r'modp_b64\.lib$',
    #r'\\icu\\icu.*\.lib$',
    #r'\\skia\\skia.*\.lib$',
    #r'ipc\.lib$',
    ## TODO: These image/coded related things should probably be renderer only?
    #r'\\libvpx\\.*\.lib$',
    #r'opus\.lib$',
    #r'libjpeg\.lib$',
    #r'qcms\.lib$',
    #r'libpng\.lib$',
    #r'zlib\\.*\.lib$',
    #r'libxml2\.lib$',
    #r'libxslt\.lib$',
    #r'\\sdch\\.*\.lib$',
    #r'\\net\\.*\.lib$',
    #r'\\nss\\.*\.lib$',
    #r'\\crypto\\.*\.lib$',
    #r'googleurl\.lib$',  # TODO: renaming.
    #r'\\sql\\.*\.lib$',
    #r'sqlite3\.lib$',

    # See comment in .cc for explanation.
    r'split_dll_fake_entry\.obj$',

    # To get DLL version information in all.
    r'chrome_dll_version.*\.res$',

    # System and third party import libs.
    r'^advapi32\.lib$',
    r'^atlthunk\.lib$',
    r'^chrome\.user32\.delay\.lib$',
    r'^comctl32\.lib$',
    r'^comdlg32\.lib$',
    r'^crypt32\.lib$',
    r'^d2d1\.lib$',
    r'^d3d9\.lib$',
    r'^dbghelp\.lib$',
    r'^delayimp\.lib$',
    r'^dinput8\.lib$',
    r'^dnsapi\.lib$',
    r'^dwmapi\.lib$',
    r'^dxva2\.lib$',
    r'ffmpegsumo\.lib$',
    r'^gdi32\.lib$',
    r'libcmt\.lib$',  # No ^ so it matches gen/allocator, etc.
    r'^imm32\.lib$',
    r'^iphlpapi\.lib$',
    r'^kernel32\.lib$',
    r'^locationapi\.lib$',
    r'^mf\.lib$',
    r'^mfplat\.lib$',
    r'^mfreadwrite\.lib$',
    r'^mfuuid\.lib$',
    r'^msimg32\.lib$',
    r'^odbc32\.lib$',
    r'^odbccp32\.lib$',
    r'^ole32\.lib$',
    r'^oleacc\.lib$',
    r'^oleaut32\.lib$',
    r'^portabledeviceguids\.lib$',
    r'^psapi\.lib$',
    r'^secur32\.lib$',
    r'^sensorsapi\.lib$',
    r'^setupapi\.lib$',
    r'^shell32\.lib$',
    r'^shlwapi\.lib$',
    r'^strmiids\.lib$',
    r'^user32\.winxp\.lib$',
    r'^usp10\.lib$',
    r'^uuid\.lib$',
    r'^version\.lib$',
    r'^wininet\.lib$',
    r'^winmm\.lib$',
    r'^winspool\.lib$',
    r'^ws2_32\.lib$',
  ],

  # objs split out of libs. These will be extracted from whichever side
  # they're not on according to the 'parts' split, and then just the obj
  # linked into the other side. Each should be a 2-tuple, where the first is
  # a regex for the .lib name, and the second is a regex for the .obj from
  # that lib. The lib should not match anything in 'all'.
  #
  # Note: If you're considering adding something that isn't a _switches or a
  # _constants file, it'd probably be better to break the target into separate
  # .lib files.
  'all_from_libs': [
    (r'autofill_common\.lib$', r'switches\.obj$'),
    (r'\bbase\.lib$', r'string_util_constants\.obj$'),
    (r'base_static\.lib$', r'base_switches\.obj$'),
    (r'\bbase\.lib$', r'file_path_constants\.obj$'),
    (r'\bcc\.lib$', r'switches\.obj$'),
    (r'\bcommon\.lib$', r'extension_constants\.obj$'),
    (r'\bcommon\.lib$', r'extension_manifest_constants\.obj$'),
    (r'\bcommon\.lib$', r'url_constants\.obj$'),
    (r'\bcommon\.lib$', r'view_type\.obj$'),
    # It sort of looks like most of this lib could go in 'all', but there's a
    # couple registration/initialization functions that make me a bit nervous.
    (r'common_constants\.lib$', r'chrome_constants\.obj$'),
    (r'common_constants\.lib$', r'chrome_switches\.obj$'),
    (r'common_constants\.lib$', r'pref_names\.obj$'),
    (r'content_common\.lib$', r'browser_plugin_constants\.obj$'),
    (r'content_common\.lib$', r'content_constants\.obj$'),
    (r'content_common\.lib$', r'content_switches\.obj$'),
    (r'content_common\.lib$', r'media_stream_options\.obj$'),
    (r'content_common\.lib$', r'page_zoom\.obj$'),
    (r'content_common\.lib$', r'url_constants\.obj$'),
    (r'gl_wrapper\.lib$', r'gl_switches\.obj$'),
    # TODO(scottmg): This one is not solely constants, but looks safe.
    (r'libjingle_webrtc_common\.lib$', r'mediaconstraintsinterface\.obj$'),
    (r'\bmedia\.lib$', r'audio_manager_base\.obj$'),
    (r'\bmedia\.lib$', r'media_switches\.obj$'),
    # TODO(scottmg): This one is not solely constants, but looks safe.
    (r'\bnet\.lib$', r'http_request_headers\.obj$'),
    (r'ppapi_shared\.lib$', r'ppapi_switches\.obj$'),
    (r'printing\.lib$', r'print_job_constants\.obj$'),
    (r'skia\.lib$', r'skunpremultiply\.obj$'),
    (r'\bui\.lib$', r'clipboard_constants\.obj$'),
    (r'\bui\.lib$', r'favicon_size\.obj$'),
    (r'\bui\.lib$', r'ui_base_switches\.obj$'),
    (r'webkit.*plugins_common\.lib$', r'plugin_switches\.obj$'),
    (r'webkit.*plugins_common\.lib$', r'plugin_constants'),
    (r'webkit.*storage\.lib$', r'file_permission_policy\.obj$'),
  ],

  # This manifest will be merged with the intermediate one from the linker,
  # and embedded in both DLLs.
  'manifest': '..\\..\\chrome\\app\\chrome.dll.manifest'
}
