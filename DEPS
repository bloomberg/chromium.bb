deps = {
  "tools/gyp":
    "http://gyp.googlecode.com/svn/trunk@725",
  "build":
    "http://src.chromium.org/svn/trunk/src/build",
  "site_scons":
    "http://src.chromium.org/svn/trunk/src/site_scons",
  "testing/gtest":
    "http://googletest.googlecode.com/svn/trunk@267",
  "third_party":
    "http://nativeclient.googlecode.com/svn/trunk/src/third_party",
  "native_client_sdk":
    "http://nativeclient.googlecode.com/svn/trunk/src/native_client_sdk",
}

deps_os = {
  "win": {
    "third_party/python_24":
      "http://src.chromium.org/svn/trunk/deps/third_party/python_24@22967",
  "third_party/wix_2_0_4221":
    "http://o3d.googlecode.com/svn/trunk/googleclient/third_party/wix_2_0_4221@153",
  },
}

hooks = [
  {
    "pattern": ".",
    "action": ["python", "native_client/build/gyp_nacl",
               "native_client/build/all.gyp"],
  },
]
