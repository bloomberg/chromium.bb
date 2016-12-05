include_rules = [
  # The subdirectories in ios/ will manually allow their own include
  # directories in ios/ so we disallow all of them.
  "-ios",

  # To avoid ODR violation, direct import of these libraries is forbidden in
  # ios/DEPS and code should instead use import as if they were in a framework
  # (i.e. #import <OCHamcrest/OCHamcrest.h>).
  "-ios/third_party/earl_grey",
  "-ios/third_party/ochamcrest",

  # For unit tests.
  "+ios/testing",
  "+third_party/ocmock",
]
