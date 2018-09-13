# Blacklist component #

The goal of the blacklist component is to provide various blacklists that allow
different policies for features to consume. Below are various types of blacklist
included within the component.

## Bloom filter blacklist ##
The Bloom filter blacklist allows blocking specific strings (hosts) based on a
probabilistic data structure that represents a host as a hashed value.
Collisions are possible (false positive matches), and the consumer is
responsible for determining what action to take when a match occurs. The
implementation uses MurmurHash3 in coordination with wherever (i.e., the server
that ships it) the bloom filter is generated.

### Expected behavior ###
The consumer needs to supply a Bloom filter, the number of times to hash the
string, and the number of bits that the Bloom filter occupies. Calling Contains,
will inform the consumer whether the string is included in the Bloom filter, and
these should be considered strings that are not allowed to be used for the
consumer feature.

### Host filter ###
HostFilter uses an internal Bloom filter to blacklist host names. It uses the
Bloom filter to store blacklisted host name suffixes. Given a URL, HostFilter
will check the URL's host name for any blacklisted host suffixes. The host
filter will look for matching sub-domains and the full domain in the Bloom
filter, and will treat any match as a blacklisted host. For instance, a host
like a.b.c.d.e.chromium.org would match any of the following if they appeared in
the Bloom filter: a.b.c.d.e.chromium.org, chromium.org, e.chromium.org,
d.e.chromium.org, c.d.e.chromium.org. Note that b.c.d.e.chromium.org is not
included, as the default implementation checks the full host, and four other
sub-domains, looking at the most granular to least granular. Hosts with top
level domains of more than 6 characters are not supported.

## Opt out blacklist ##
The opt out blacklist makes decisions based on user history actions. Each user
action is evaluated based on action type, time of the evaluation, host name of
the action (can be any string representation), and previous action history.

### Expected feature behavior ###
When a feature action is allowed, the feature may perform said action. After
performing the action, the user interaction should be determined to be an opt
out (the user did not like the action) or a non-opt out (the user was not
opposed to the action). The action, type, host name, and whether it was an opt
out should be reported back to the blacklist to build user action history.

For example, a feature may wish to show an InfoBar (or different types of
InfoBars) displaying information about the page a user is on. After querying the
opt out blacklist for action eligibility, an InfoBar may be allowed to be shown.
If it is shown, the user may interact with it in a number of ways. If the user
dismisses the InfoBar, that could be considered an opt out; if the user does
not dismiss the InfoBar that could be considered a non-opt out. All of the
information related to that action should be reported to the blacklist.

### Supported evaluation policies ###
In general, policies follow a specific form: the most recent _n_ actions are
evaluated, and if _t_ or more of them are opt outs the action will not be
allowed for a specified duration, _d_. For each policy, the feature specifies
whether the policy is enabled, and, if it is, the feature specifies _n_
(history), _t_ (threshold), and _d_ (duration) for each policy.

* Session policy: This policy only applies across all types and host names, but
is limited to actions that happened within the current session. The beginning of
a session is defined as the creation of the blacklist object or when the
blacklist is cleared (see below for details on clearing the blacklist).

* Persistent policy: This policy applies across all sessions, types and host
names.

* Host policy: This policy applies across all session and types, but keeps a
separate history for each host names. This rule allows specific host names to be
prevented from having an action performed for the specific user. When this
policy is enabled, the feature specifies a number of hosts that are stored in
memory (to limit memory footprint, query time, etc.)

* Type policy: This policy applies across all session and host names, but keeps
a separate history for each type. This rule allows specific types to be
prevented from having an action performed for the specific user. The feature
specifies a set of enabled types and versions for each type. This allows
removing past versions of types to be removed from the backing store.

### Clearing the blacklist ###
Because many actions should be cleared when user clears history, the opt out
blacklist allows clearing history in certain time ranges. All entries are
cleared for the specified time range, and the data in memory is repopulated
from the backing store.
