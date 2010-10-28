This directory is supposed to represent a "typical" size of a history
and top sites database that a user will have. It does not include a
full text index data.  It should be copied to another location before
using in a test.

It was generated with
  generate_profile --top-sites 50000 "Default"

The unit test HistoryProfileTest.TypicalProfileVersion tests that the version
of this profile is the same that the application is expecting without
migration. Otherwise, migration time will be counted in some of the performance
tests.
