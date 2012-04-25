define([ 'sprites/Transform' ], function (Transform) {
    return function scale(frameIndex, objectIndex) {
        var x = Math.cos((objectIndex + frameIndex * (objectIndex + 1)) / 100) * 200 + 100;
        var y = Math.sin((objectIndex + frameIndex * (objectIndex + 1)) / 100) * 200 + 100;

        return new Transform({
            x: x,
            y: y
        });
    };
});
