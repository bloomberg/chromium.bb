# This dict determines how chrome.dll is split into multiple parts.
{
  'parts': [
    # These sections are matched in order, and a matching input will go into
    # the part for the last block that matches. Inputs are lower()d before
    # the regex is run.

    # chrome0.dll.
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
}
