use_relative_paths = True

vars = {
  "chromium_url": "https://chromium.googlesource.com",

  # When changing these, also update the svn revisions in deps_revisions.gni
  "clang_format_revision": "0653eee0c81ea04715c635dd0885e8096ff6ba6d",
  "libcxx_revision":       "640fa255b9c7441eeace90f96e237ca68bd8c7b6",
  "libcxxabi_revision":    "794d4c0c33d99585ae99a069e99ff2c06aa98b56",
  "libunwind_revision":    "94edd59b16d2084a62699290f9cfdf120d74eedf",
}

deps = {
  "clang_format/script":
    Var("chromium_url") + "/chromium/llvm-project/cfe/tools/clang-format.git@" +
    Var("clang_format_revision"),
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
