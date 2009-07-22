deps = {
  "tools/gyp":
    "http://gyp.googlecode.com/svn/trunk@546",
  "site_scons":
    "http://src.chromium.org/svn/trunk/src/site_scons",
  "testing/gtest":
    "http://googletest.googlecode.com/svn/trunk@267",
}

hooks = [
  {
    "pattern": "\\.gypi?$[|[/\\\]src[/\\\]tools[/\\\]gyp[/\\\]",
    "action": ["python", "tools/gyp/gyp_dogfood",
               "native_client/build/all.gyp"],
  },
]
