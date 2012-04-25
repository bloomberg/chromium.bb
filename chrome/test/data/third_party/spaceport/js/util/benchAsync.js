define([ 'util/ensureCallback' ], function (ensureCallback) {
    // Benchmarks fn until maxTime ms has passed.  Returns approximate number
    // of operations performed in that time ('score').
    function benchAsync(maxTime, fn, callback) {
        if (typeof fn !== 'function') {
            throw new TypeError('Argument must be a function');
        }

        var operationCount = 0;
        var startTime;

        function next() {
            fn(operationCount, function () {
                ++operationCount;

                var endTime = Date.now();
                if (endTime - startTime >= maxTime) {
                    var score = operationCount / (endTime - startTime) * maxTime;
                    return callback(null, score);
                } else {
                    return next();
                }
            });
        }

        setTimeout(function () {
            startTime = Date.now();
            next();
        }, 0);
    }

    return benchAsync;
});
