use_relative_paths = True

vars = {
  "chromium_url": "https://chromium.googlesource.com",

  "clang_format_rev": "0653eee0c81ea04715c635dd0885e8096ff6ba6d",   # r302580
  "libcxx_revision": "27c341db41bc9df5c6f19cde65f002d6f1c2eb3c",    # r323563
  "libcxxabi_revision": "e1601db2504857d44db88a5d4e2ca50b32bbb7d9", # r323495
  "libunwind_revision": "86ab23972978242b6f9e27cebc239f3e8428b1af", # r323499
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
