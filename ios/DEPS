include_rules = [
  # The subdirectories in ios/ will manually allow their own include
  # directories in ios/ so we disallow all of them.
  "-ios",

  # To avoid ODR violation, direct import of ios/third_party/ochamcrest
  # is forbidden in ios/DEPS and code should instead use import as if
  # OCHamcrest was in a framework (i.e. #import <OCHamcrest/OCHamcrest.h>).
  "-ios/third_party/ochamcrest",

  # For unit tests.
  "+third_party/ocmock",
]
