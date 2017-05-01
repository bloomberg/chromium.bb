use_relative_paths = True

vars = {
  "chromium_url": "https://chromium.googlesource.com",

  "clang_format_rev": "c09c8deeac31f05bd801995c475e7c8070f9ecda",   # r296408
  "libcxx_revision": "57c405955f0abd56f81152ead9e2344b532276ad",    # r301132
  "libcxxabi_revision": "700fa3562ffeb4e6416bb07fa731ea98105604d3", # r300925
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
}
