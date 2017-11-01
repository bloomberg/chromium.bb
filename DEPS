use_relative_paths = True

vars = {
  "chromium_url": "https://chromium.googlesource.com",

  "clang_format_rev": "0653eee0c81ea04715c635dd0885e8096ff6ba6d",   # r302580
  "libcxx_revision": "f56f1bba1ade4a408d403ff050d50e837bae47df",    # r317124
  "libcxxabi_revision": "05ba3281482304ae8de31123a594972a495da06d", # r313308
  "libunwind_revision": "fc0a910c25d5415dd72ba9451b7cba380e3cc1e7", # r317125
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
