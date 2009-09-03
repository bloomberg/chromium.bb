deps = {
  "tools/gyp":
    "http://gyp.googlecode.com/svn/trunk@636",
  "build":
    "http://src.chromium.org/svn/trunk/src/build",
  "site_scons":
    "http://src.chromium.org/svn/trunk/src/site_scons",
  "testing/gtest":
    "http://googletest.googlecode.com/svn/trunk@267",
  "third_party":
    "http://nativeclient.googlecode.com/svn/trunk/src/third_party",
}

hooks = [
  {
    "pattern": "\\.gypi?$|[/\\\]tools[/\\\]gyp[/\\\]",
    "action": ["python", "native_client/build/gyp_nacl",
               "native_client/build/all.gyp"],
  },
]
