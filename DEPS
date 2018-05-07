use_relative_paths = True

vars = {
  "chromium_url": "https://chromium.googlesource.com",

  # When changing these, also update the svn revisions in deps_revisions.gni
  "clang_format_revision": "0653eee0c81ea04715c635dd0885e8096ff6ba6d",
  "libcxx_revision":       "ece1de8658d749e19c12cacd4458cc330eca94e3",
  "libcxxabi_revision":    "05a73941f3fb177c4a891d4ff2a4ed5785e3b80c",
  "libunwind_revision":    "0f3cbe4123f8afacd646bd4f5414aa6528ef8129",
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
