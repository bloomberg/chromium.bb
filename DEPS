use_relative_paths = True

vars = {
  "chromium_url": "https://chromium.googlesource.com",

  "clang_format_rev": "0653eee0c81ea04715c635dd0885e8096ff6ba6d",   # r302580
  "libcxx_revision": "8864da5e7823f79f801512ec4dbcca89feff7a23",    # r323390
  "libcxxabi_revision": "9a02f50fc1d2acde806c69085ad979a685cb0694", # r323397
  "libunwind_revision": "98762d639111593699b509ab6bc20ebed0e41352", # r323141
}

deps = {
  "clang_format/script":
    Var("chromium_url") + "/chromium/llvm-project/cfe/tools/clang-format.git@" +
    Var("clang_format_rev"),
  "third_party/libc++/trunk":
    Var("chromium_url") + "/chromium/llvm-project/libcxx.git" + "@" +
    Var("libcxx_revision"),
  "third_party/libc++abi/trunk":
    Var("chromium_url") + "/chromium/llvm-project/libcxxabi.git" + "@" +
    Var("libcxxabi_revision"),
  "third_party/libunwind/trunk":
    Var("chromium_url") + "/external/llvm.org/libunwind.git" + "@" +
    Var("libunwind_revision"),
}
